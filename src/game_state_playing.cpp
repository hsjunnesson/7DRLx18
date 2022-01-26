#include "action_binds.h"
#include "array.h"
#include "game.h"
#include "dungen.h"
#include "color.inl"

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


namespace {
const float LEVEL_Z_LAYER = -5.0f;
const float ITEM_Z_LAYER = -4.0f;
const float MOB_Z_LAYER = -3.0f;
} // namespace


namespace game {
using namespace math;

/// Return the world coordinate of the center of a tile at pos.
const glm::vec3 pos_to_world(const Game &game, const uint32_t pos) {
    uint32_t x, y;
    coord(pos, x, y, game.level->max_width);
    
    glm::vec3 w {x, y, 0.0f};
    w *= game.params->tilesize();

    return w;
}

/// Return the screen coordinate of the center of a tile at pos, scaled by zoom and render scale.
const glm::vec2 pos_to_screen(const engine::Engine &engine, const Game &game, const uint32_t pos) {
    uint32_t x, y;
    coord(pos, x, y, game.level->max_width);
    
    glm::vec2 w {x, y};
    w *= game.params->tilesize() * engine.camera_zoom * engine.render_scale;
    w += game.params->tilesize() / 2.0f;

    return w;
}

/// Centers the view on the tile at pos.
void center_view_to_pos(engine::Engine &engine, const Game &game, const uint32_t pos) {
    glm::vec2 w = pos_to_screen(engine, game, pos);
    engine::move_camera(engine, w.x - engine.window_rect.size.x / 2, w.y - engine.window_rect.size.y / 2);
}

/// Utility to add a sprite to the world.
uint64_t add_sprite(engine::Sprites &sprites, const char *sprite_name, uint32_t tilesize, const uint32_t pos, const uint32_t max_width, const float z_layer, Color4f color = color::white) {
    const engine::Sprite sprite = engine::add_sprite(sprites, sprite_name);

    uint32_t x, y;
    coord(pos, x, y, max_width);

    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, {x * tilesize, y * tilesize, z_layer});
    transform = glm::scale(transform, glm::vec3((float)sprite.atlas_rect->size.x, (float)sprite.atlas_rect->size.y, 1.0f));
    engine::transform_sprite(sprites, sprite.id, transform);
    engine::color_sprite(sprites, sprite.id, color);

    return sprite.id;
}

void player_move(int32_t x, int32_t y) {

}

void player_wait() {

}

void game_state_playing_started(engine::Engine &engine, Game &game) {
    // Create level tiles
    {
        hash::clear(game.level->tiles_sprite_ids);

        for (auto iter = hash::begin(game.level->tiles); iter != hash::end(game.level->tiles); ++iter) {
            uint64_t pos = iter->key;

            Tile tile = static_cast<Tile>(iter->value);
            const char *sprite_name = tile_sprite_name(tile);
            uint64_t sprite_id = add_sprite(*engine.sprites, sprite_name, game.params->tilesize(), pos, game.level->max_width, LEVEL_Z_LAYER);
            hash::set(game.level->tiles_sprite_ids, pos, sprite_id);
        }
    }

    // Create player
    {
        game.player_mob.pos = game.level->stairs_up_pos;
        uint64_t sprite_id = add_sprite(*engine.sprites, "farmer", game.params->tilesize(), game.player_mob.pos, game.level->max_width, MOB_Z_LAYER, color::peach);
        game.player_mob.sprite_id = sprite_id;
        center_view_to_pos(engine, game, game.player_mob.pos);

        const engine::Sprite *player_sprite = get_sprite(*engine.sprites, sprite_id);
        const glm::vec3 player_world_pos = player_sprite->transform[3];
        const uint32_t adjacent_pos = index_offset(game.player_mob.pos, 3, 0, game.level->max_width);
        const glm::vec3 adjacent_world_pos = pos_to_world(game, adjacent_pos);
        uint64_t animation_id = animate_sprite_position(*engine.sprites, game.player_mob.sprite_id, {adjacent_world_pos.x, adjacent_world_pos.y, MOB_Z_LAYER}, 3.0f);
        log_debug("Created animation %" PRIx64, animation_id);
    }
}

void game_state_playing_on_input(engine::Engine &engine, Game &game, engine::InputCommand &input_command) {
    assert(game.action_binds != nullptr);

    if (input_command.input_type == engine::InputType::Mouse) {
        switch (input_command.mouse_state.mouse_action) {
        case engine::MouseAction::MouseMoved: {
            if (input_command.mouse_state.mouse_left_state == engine::TriggerState::Pressed) {
                int32_t x = input_command.mouse_state.mouse_relative_motion.x;
                int32_t y = input_command.mouse_state.mouse_relative_motion.y;
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
        case ActionBindEntry::MOVE_N: {
            if (pressed || repeated) {
                player_move(0, 1);
            }
            break;
        }
        case ActionBindEntry::MOVE_NE: {
            if (pressed || repeated) {
                player_move(1, 1);
            }
            break;
        }
        case ActionBindEntry::MOVE_E: {
            if (pressed || repeated) {
                player_move(1, 0);
            }
            break;
        }
        case ActionBindEntry::MOVE_SE: {
            if (pressed || repeated) {
                player_move(1, -1);
            }
            break;
        }
        case ActionBindEntry::MOVE_S: {
            if (pressed || repeated) {
                player_move(0, -1);
            }
            break;
        }
        case ActionBindEntry::MOVE_SW: {
            if (pressed || repeated) {
                player_move(-1, -1);
            }
            break;
        }
        case ActionBindEntry::MOVE_W: {
            if (pressed || repeated) {
                player_move(-1, 0);
            }
            break;
        }
        case ActionBindEntry::MOVE_NW: {
            if (pressed || repeated) {
                player_move(-1, 1);
            }
            break;
        }
        case ActionBindEntry::INTERACT: {
            log_debug("Not implemented action INTERACT");
            break;
        }
        case ActionBindEntry::WAIT: {
            if (pressed || repeated) {
                player_wait();
            }
            break;
        }
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
    (void)engine;
    (void)game;
    (void)t;
    (void)dt;

    const Array<engine::SpriteAnimation> done_animations = done_sprite_animations(*engine.sprites);
    for (const engine::SpriteAnimation *iter = array::begin(done_animations); iter != array::end(done_animations); ++iter) {
        log_debug("Completed animation %" PRIx64, iter->animation_id);
    }
}

void game_state_playing_render(engine::Engine &engine, Game &game) {
    (void)engine;

    if (game.present_hud) {
    }
}

} // namespace game
