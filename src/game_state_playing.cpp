#include "action_binds.h"
#include "array.h"
#include "game.h"
#include "dungen.h"
#include "color.inl"

#pragma warning(push, 0)
#include <engine/engine.h>
#include <engine/input.h>
#include <engine/sprites.h>
#include <engine/log.h>
#include <proto/game.pb.h>

#include <array.h>
#include <hash.h>

#include <imgui.h>
#include <cassert>
#include <limits>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#pragma warning(pop)


namespace {
const float LEVEL_Z_LAYER = -5.0f;
const float ITEM_Z_LAYER = -4.0f;
const float MOB_Z_LAYER = -3.0f;
const float MOB_WALK_SPEED = 0.1f;
} // namespace


namespace game {
using namespace math;

/// Return the world position of the center of a tile at coord.
const glm::vec3 tile_index_to_world_position(const Game &game, const uint32_t index) {
    uint32_t x, y;
    coord(index, x, y, game.level->max_width);
    
    glm::vec3 w {x, y, 0.0f};
    w *= game.params->tilesize();

    return w;
}

/// Return the screen position of the center of a tile at coord, scaled by zoom and render scale.
const glm::vec2 tile_index_to_screen_position(const engine::Engine &engine, const Game &game, const uint32_t index) {
    uint32_t x, y;
    coord(index, x, y, game.level->max_width);
    
    glm::vec2 w {x, y};
    w *= game.params->tilesize() * engine.camera_zoom * engine.render_scale;
    w += game.params->tilesize() / 2.0f;

    return w;
}

/// Whether a tile at index is traversible.
bool is_traversible(const Game &game, const uint32_t index) {
	const Tile &tile = hash::get(game.level->tiles, index, Tile::None);
    
    switch (tile) {
    case Tile::StairsDown:
    case Tile::StairsUp:
    case Tile::Floor:
        return true;
    default:
        return false;
    }
}

/// Centers the view on the tile at index.
void center_view_to_tile_index(engine::Engine &engine, const Game &game, const uint32_t index) {
    glm::vec2 w = tile_index_to_screen_position(engine, game, index);
    engine::move_camera(engine, (int32_t)w.x - engine.window_rect.size.x / 2, (int32_t)w.y - engine.window_rect.size.y / 2);
}

/// Utility to add a sprite to the game.
uint64_t add_sprite(engine::Sprites &sprites, const char *sprite_name, uint32_t tilesize, const uint32_t index, const uint32_t max_width, const float z_layer, Color4f color = color::white) {
    const engine::Sprite sprite = engine::add_sprite(sprites, sprite_name);

    uint32_t x, y;
    coord(index, x, y, max_width);

    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, {x * tilesize, y * tilesize, z_layer});
    transform = glm::scale(transform, glm::vec3((float)sprite.atlas_rect->size.x, (float)sprite.atlas_rect->size.y, 1.0f));
    engine::transform_sprite(sprites, sprite.id, transform);
    engine::color_sprite(sprites, sprite.id, color);

    return sprite.id;
}

void mob_walk(engine::Engine &engine, Game &game, Mob &mob, int32_t xoffset, int32_t yoffset) {
    uint32_t new_index = index_offset(mob.index, xoffset, yoffset, game.level->max_width);
	bool legal_move = is_traversible(game, new_index);

    if (legal_move) {
		glm::vec3 to_position = tile_index_to_world_position(game, new_index);
		to_position.z = MOB_Z_LAYER;
		uint64_t animation_id = engine::animate_sprite_position(*engine.sprites, mob.sprite_id, to_position, MOB_WALK_SPEED);
        if (animation_id != 0) {
            hash::set(game.processing_animations, animation_id, true);
        }

		const uint32_t old_index = mob.index;
		mob.index = new_index;

        // Unhide and hide level tiles
        {
            const float fade_speed = MOB_WALK_SPEED / 2.0f;

            const Tile old_tile = hash::get(game.level->tiles, old_index, Tile::None);
            const Tile new_tile = hash::get(game.level->tiles, new_index, Tile::None);

            if (old_tile != Tile::None) {
                const uint64_t old_index_sprite_id = hash::get(game.level->tiles_sprite_ids, old_index, (uint64_t)0);

                if (old_index_sprite_id > 0) {
                    const engine::Sprite *sprite = engine::get_sprite(*engine.sprites, old_index_sprite_id);
                    if (sprite) {
                        Color4f color = sprite->color;
                        color.a = 1.0f;
                        engine::animate_sprite_color(*engine.sprites, sprite->id, color, fade_speed, fade_speed);
                    }
                }
            }

            if (new_tile != Tile::None) {
                const uint64_t new_index_sprite_id = hash::get(game.level->tiles_sprite_ids, new_index, (uint64_t)0);

                if (new_index_sprite_id > 0) {
                    const engine::Sprite *sprite = engine::get_sprite(*engine.sprites, new_index_sprite_id);
                    if (sprite) {
                        Color4f color = sprite->color;
                        color.a = 0.0f;
                        engine::animate_sprite_color(*engine.sprites, sprite->id, color, fade_speed, fade_speed);
                    }
                }
            }
        }
    }
}

void player_wait() {

}

void game_state_playing_started(engine::Engine &engine, Game &game) {
    // Create level tiles
    {
        hash::clear(game.level->tiles_sprite_ids);

        for (auto iter = hash::begin(game.level->tiles); iter != hash::end(game.level->tiles); ++iter) {
            uint32_t index = (uint32_t)iter->key;

            Tile tile = static_cast<Tile>(iter->value);
            const char *sprite_name = tile_sprite_name(tile);
			uint64_t sprite_id = add_sprite(*engine.sprites, sprite_name, game.params->tilesize(), index, game.level->max_width, LEVEL_Z_LAYER);
			hash::set(game.level->tiles_sprite_ids, index, sprite_id);
        }
    }

    // Create player
    {
        game.player_mob.index = game.level->stairs_up_index;
        uint64_t sprite_id = add_sprite(*engine.sprites, "farmer", game.params->tilesize(), game.player_mob.index, game.level->max_width, MOB_Z_LAYER, color::peach);
        game.player_mob.sprite_id = sprite_id;
        center_view_to_tile_index(engine, game, game.player_mob.index);

        const uint64_t stairs_sprite_id = hash::get(game.level->tiles_sprite_ids, game.level->stairs_up_index, (uint64_t)0);
        if (stairs_sprite_id) {
            const engine::Sprite *stairs_sprite = engine::get_sprite(*engine.sprites, stairs_sprite_id);
            Color4f color = stairs_sprite->color;
            color.a = 0.0f;
            engine::color_sprite(*engine.sprites, stairs_sprite_id, color);
        }
    }
}

void game_state_playing_on_input(engine::Engine &engine, Game &game, engine::InputCommand &input_command) {
    assert(game.action_binds != nullptr);

    if (input_command.input_type == engine::InputType::Mouse) {
        switch (input_command.mouse_state.mouse_action) {
        case engine::MouseAction::MouseMoved: {
            if (input_command.mouse_state.mouse_left_state == engine::TriggerState::Pressed) {
                int32_t x = (int32_t)input_command.mouse_state.mouse_relative_motion.x;
                int32_t y = (int32_t)input_command.mouse_state.mouse_relative_motion.y;
                engine::offset_camera(engine, -x, y);
            }
            break;
        }
        case engine::MouseAction::MouseTrigger: {
            break;
        }
        default:
            break;
        }
    }

    if (input_command.input_type == engine::InputType::Key) {
        bool pressed = input_command.key_state.trigger_state == engine::TriggerState::Pressed;
        // bool released = input_command.key_state.trigger_state == engine::TriggerState::Released;
        bool repeated = input_command.key_state.trigger_state == engine::TriggerState::Repeated;

        ActionBindEntry_Action action = action_for_bind(*game.action_binds, (ActionBindEntry_Bind)input_command.key_state.keycode);
        switch (action) {
        case ActionBindEntry::QUIT: {
            if (pressed) {
                transition(engine, &game, GameState::Quitting);
            }
            break;
        }
        case ActionBindEntry::MENU: {
            log_debug("Not implemented action MENU");
            break;
        }
        case ActionBindEntry::DEBUG_HUD: {
            if (pressed) {
                game.present_hud = !game.present_hud;
            }
            break;
        }
        case ActionBindEntry::MOVE_N:
        case ActionBindEntry::MOVE_NE:
        case ActionBindEntry::MOVE_E:
        case ActionBindEntry::MOVE_SE:
        case ActionBindEntry::MOVE_S:
        case ActionBindEntry::MOVE_SW:
        case ActionBindEntry::MOVE_W:
        case ActionBindEntry::MOVE_NW:
        case ActionBindEntry::INTERACT:
        case ActionBindEntry::WAIT:
            if (pressed || repeated) {
                game.queued_action = action;
            }
            break;
        default:
            break;
        }
    }

    if (input_command.input_type == engine::InputType::Scroll) {
        // Zoom
        if (fabs(input_command.scroll_state.y_offset) > FLT_EPSILON) {
            if (input_command.scroll_state.y_offset > 0.0f) {
                engine::zoom_camera(engine, engine.camera_zoom * 2.0f);
            } else {
                engine::zoom_camera(engine, engine.camera_zoom * 0.5f);
            }
        }
    }
}

void game_state_playing_update(engine::Engine &engine, Game &game, float t, float dt) {
    (void)game;
    (void)t;
    (void)dt;

    // Check done with processing animations
    {
        const Array<engine::SpriteAnimation> done_animations = done_sprite_animations(*engine.sprites);
        for (const engine::SpriteAnimation *iter = array::begin(done_animations); iter != array::end(done_animations); ++iter) {
            uint64_t animation_id = iter->animation_id;
            if (hash::has(game.processing_animations, animation_id)) {
                hash::remove(game.processing_animations, animation_id);
            }
        }

        if (game.processing_turn) {
            int processing_animations_count = 0;
            for (auto iter = hash::begin(game.processing_animations); iter != hash::end(game.processing_animations); ++iter) {
                ++processing_animations_count;
            }
            if (processing_animations_count == 0) {
                game.processing_turn = false;
            }
        }
    }

    // Process queued action
    if (!game.processing_turn && game.queued_action != ActionBindEntry_Action_ACTION_UNKNOWN) {
        bool invalid_action = false;

        switch (game.queued_action) {
        case ActionBindEntry::MOVE_N: {
            mob_walk(engine, game, game.player_mob, 0, 1);
            break;
        }
        case ActionBindEntry::MOVE_NE: {
            mob_walk(engine, game, game.player_mob, 1, 1);
            break;
        }
        case ActionBindEntry::MOVE_E: {
            mob_walk(engine, game, game.player_mob, 1, 0);
            break;
        }
        case ActionBindEntry::MOVE_SE: {
            mob_walk(engine, game, game.player_mob, 1, -1);
            break;
        }
        case ActionBindEntry::MOVE_S: {
            mob_walk(engine, game, game.player_mob, 0, -1);
            break;
        }
        case ActionBindEntry::MOVE_SW: {
            mob_walk(engine, game, game.player_mob, -1, -1);
            break;
        }
        case ActionBindEntry::MOVE_W: {
            mob_walk(engine, game, game.player_mob, -1, 0);
            break;
        }
        case ActionBindEntry::MOVE_NW: {
            mob_walk(engine, game, game.player_mob, -1, 1);
            break;
        }
        case ActionBindEntry::WAIT: {
            player_wait();
            break;
        }
        default: {
            const google::protobuf::EnumDescriptor *descriptor = game::ActionBindEntry_Action_descriptor();
            const char *action_name = descriptor->FindValueByNumber(game.queued_action)->name().c_str();
            log_debug("Not implemented action: %s", action_name);
            invalid_action = true;
            break;
        }
        }

        game.queued_action = ActionBindEntry_Action_ACTION_UNKNOWN;

        if (!invalid_action) {
            game.player_mob.energy = 0.0f;
            game.processing_turn = true;
        }
    }
}

void game_state_playing_render(engine::Engine &engine, Game &game) {
    (void)engine;

    if (game.present_hud) {
    }
}

} // namespace game
