#include "action_binds.h"
#include "color.inl"
#include "dungen.h"
#include "game.h"
#include "mob.h"

#pragma warning(push, 0)
#include <engine/engine.h>
#include <engine/input.h>
#include <engine/log.h>
#include <engine/sprites.h>
#include <proto/game.pb.h>

#include <array.h>
#include <hash.h>
#include <string_stream.h>

#include <cassert>
#include <imgui.h>
#include <limits>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>

#include <imgui.h>
#pragma warning(pop)

namespace {
const int32_t LEVEL_Z_LAYER = -5;
const int32_t ITEM_Z_LAYER = -4;
const int32_t MOB_Z_LAYER = -3;
const float MOB_WALK_SPEED = 0.1f;
} // namespace

namespace game {
using namespace math;
using namespace foundation;

/// Return the world position of the center of a tile at coord.
const Vector2 tile_index_to_world_position(const Game &game, const int32_t index) {
    int32_t x, y;
    coord(index, x, y, game.level->max_width);
    int32_t tilesize = game.params->tilesize();

    return Vector2{(int32_t)x * tilesize, (int32_t)y * tilesize};
}

/// Return the screen position of the center of a tile at coord, scaled by zoom and render scale.
const Vector2 tile_index_to_screen_position(const engine::Engine &engine, const Game &game, const int32_t index) {
    int32_t x, y;
    coord(index, x, y, game.level->max_width);
    int32_t tilesize = game.params->tilesize();

    float fx = x * tilesize * engine.camera_zoom * engine.render_scale + (tilesize / 2.0f);
    float fy = y * tilesize * engine.camera_zoom * engine.render_scale + (tilesize / 2.0f);

    return Vector2{(int32_t)floorf(fx), (int32_t)floorf(fy)};
}

/// Whether a tile at index is traversible.
bool is_traversible(const Game &game, const int32_t index) {
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
void center_view_to_tile_index(engine::Engine &engine, const Game &game, const int32_t index) {
    Vector2 pos = tile_index_to_screen_position(engine, game, index);
    engine::move_camera(engine, pos.x - engine.window_rect.size.x / 2, pos.y - engine.window_rect.size.y / 2);
}

/// Utility to add a sprite to the game.
uint64_t add_sprite(engine::Sprites &sprites, const char *sprite_name, int32_t tilesize, const int32_t index, const int32_t max_width, const float z_layer, Color4f color = color::white) {
    const engine::Sprite sprite = engine::add_sprite(sprites, sprite_name);

    int32_t x, y;
    coord(index, x, y, max_width);

    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, {x * tilesize, y * tilesize, z_layer});
    transform = glm::scale(transform, glm::vec3(sprite.atlas_rect->size.x, sprite.atlas_rect->size.y, 1));
    engine::transform_sprite(sprites, sprite.id, Matrix4f(glm::value_ptr(transform)));
    engine::color_sprite(sprites, sprite.id, color);

    return sprite.id;
}

void mob_walk(engine::Engine &engine, Game &game, Mob &mob, int32_t xoffset, int32_t yoffset) {
    int32_t new_tile_index = index_offset(mob.tile_index, xoffset, yoffset, game.level->max_width);
    bool legal_move = is_traversible(game, new_tile_index);

    if (legal_move) {
        const Vector2 world_position = tile_index_to_world_position(game, new_tile_index);
        const Vector3 to_position = {world_position.x, world_position.y, MOB_Z_LAYER};
        uint64_t animation_id = engine::animate_sprite_position(*engine.sprites, mob.sprite_id, to_position, MOB_WALK_SPEED);
        if (animation_id != 0) {
            hash::set(game.processing_animations, animation_id, true);
        }

        const int32_t old_tile_index = mob.tile_index;
        mob.tile_index = new_tile_index;

        // Unhide and hide level tiles
        {
            const float fade_speed = MOB_WALK_SPEED / 2.0f;

            const Tile old_tile = hash::get(game.level->tiles, old_tile_index, Tile::None);
            const Tile new_tile = hash::get(game.level->tiles, new_tile_index, Tile::None);

            if (old_tile != Tile::None) {
                const uint64_t old_index_sprite_id = hash::get(game.level->tiles_sprite_ids, old_tile_index, (uint64_t)0);

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
                const uint64_t new_index_sprite_id = hash::get(game.level->tiles_sprite_ids, new_tile_index, (uint64_t)0);

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

void game_state_playing_enter(engine::Engine &engine, Game &game) {
    // Create level tiles
    {
        for (auto iter = hash::begin(game.level->tiles); iter != hash::end(game.level->tiles); ++iter) {
            int32_t index = (int32_t)iter->key;

            Tile tile = static_cast<Tile>(iter->value);
            const char *sprite_name = tile_sprite_name(tile);
            uint64_t sprite_id = add_sprite(*engine.sprites, sprite_name, game.params->tilesize(), index, game.level->max_width, LEVEL_Z_LAYER);
            hash::set(game.level->tiles_sprite_ids, index, sprite_id);
        }
    }

    // Create player
    {
        game.player_mob = MAKE_NEW(game.allocator, Mob);

        game.player_mob->tile_index = game.level->stairs_up_index;
        uint64_t sprite_id = add_sprite(*engine.sprites, "farmer", game.params->tilesize(), game.player_mob->tile_index, game.level->max_width, MOB_Z_LAYER, color::peach);
        game.player_mob->sprite_id = sprite_id;
        center_view_to_tile_index(engine, game, game.player_mob->tile_index);

        const uint64_t stairs_sprite_id = hash::get(game.level->tiles_sprite_ids, game.level->stairs_up_index, (uint64_t)0);
        if (stairs_sprite_id) {
            const engine::Sprite *stairs_sprite = engine::get_sprite(*engine.sprites, stairs_sprite_id);
            Color4f color = stairs_sprite->color;
            color.a = 0.0f;
            engine::color_sprite(*engine.sprites, stairs_sprite_id, color);
        }
    }

    // Create enemy sprites
    {
        for (auto it = hash::begin(game.enemy_mobs); it != hash::end(game.enemy_mobs); ++it) {
            Mob &mob = *it->value;
            uint64_t sprite_id = add_sprite(*engine.sprites, string_stream::c_str(*mob.mob_template->sprite_name), game.params->tilesize(), mob.tile_index, game.level->max_width, MOB_Z_LAYER, mob.mob_template->sprite_color);
            mob.sprite_id = sprite_id;
        }
    }
}

void game_state_playing_leave(engine::Engine &engine, Game &game) {
    if (game.level) {
        for (auto iter = hash::begin(game.level->tiles_sprite_ids); iter != hash::end(game.level->tiles_sprite_ids); ++iter) {
            engine::remove_sprite(*engine.sprites, iter->value);
        }

        MAKE_DELETE(game.allocator, Level, game.level);
        game.level = nullptr;
    }

    for (auto it = hash::begin(game.enemy_mobs); it != hash::end(game.enemy_mobs); ++it) {
        engine::remove_sprite(*engine.sprites, it->value->sprite_id);
        MAKE_DELETE(game.allocator, Mob, it->value);
    }
    hash::clear(game.enemy_mobs);

    if (game.player_mob) {
        engine::remove_sprite(*engine.sprites, game.player_mob->sprite_id);
        MAKE_DELETE(game.allocator, Mob, game.player_mob);
        game.player_mob = nullptr;
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
                game.camera_locked_on_player = false;
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
        case ActionBindEntry::EDITOR: {
            if (pressed) {
                game.presenting_editor = !game.presenting_editor;
            }
            break;
        }
        case ActionBindEntry::SHOW_IMGUI_DEMO: {
            if (pressed) {
                game.presenting_imgui_demo = !game.presenting_imgui_demo;
            }
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
            float zoom = engine.camera_zoom;

            if (input_command.scroll_state.y_offset > 0.0f) {
                zoom = zoom * 2.0f;
            } else {
                zoom = zoom * 0.5f;
            }

            zoom = std::clamp(zoom, 0.125f, 4.0f);
            engine::zoom_camera(engine, zoom);
        }
    }
}

void game_state_playing_update(engine::Engine &engine, Game &game, float t, float dt) {
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
            mob_walk(engine, game, *game.player_mob, 0, 1);
            game.camera_locked_on_player = true;
            break;
        }
        case ActionBindEntry::MOVE_NE: {
            mob_walk(engine, game, *game.player_mob, 1, 1);
            game.camera_locked_on_player = true;
            break;
        }
        case ActionBindEntry::MOVE_E: {
            mob_walk(engine, game, *game.player_mob, 1, 0);
            game.camera_locked_on_player = true;
            break;
        }
        case ActionBindEntry::MOVE_SE: {
            mob_walk(engine, game, *game.player_mob, 1, -1);
            game.camera_locked_on_player = true;
            break;
        }
        case ActionBindEntry::MOVE_S: {
            mob_walk(engine, game, *game.player_mob, 0, -1);
            game.camera_locked_on_player = true;
            break;
        }
        case ActionBindEntry::MOVE_SW: {
            mob_walk(engine, game, *game.player_mob, -1, -1);
            game.camera_locked_on_player = true;
            break;
        }
        case ActionBindEntry::MOVE_W: {
            mob_walk(engine, game, *game.player_mob, -1, 0);
            game.camera_locked_on_player = true;
            break;
        }
        case ActionBindEntry::MOVE_NW: {
            mob_walk(engine, game, *game.player_mob, -1, 1);
            game.camera_locked_on_player = true;
            break;
        }
        case ActionBindEntry::WAIT: {
            player_wait();
            game.camera_locked_on_player = true;
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
            game.player_mob->energy = 0;
            game.processing_turn = true;
        }
    }

    // Update camera
    if (game.camera_locked_on_player) {
        const Vector2 pos = tile_index_to_screen_position(engine, game, game.player_mob->tile_index);
        const glm::vec2 to_pos = {pos.x - engine.window_rect.size.x / 2.0f, pos.y - engine.window_rect.size.y / 2.0f};
        const glm::vec2 from_pos = {engine.camera_offset.x, engine.camera_offset.y};
        const glm::vec2 direction = to_pos - from_pos;
        const float length = fabs(glm::length(direction));
        const int32_t tilesize = game.params->tilesize();
        const float min_length = tilesize * 1.0f;
        const float max_length = tilesize * 24.0f;
        const float min_speed = 0.25f;
        const float max_speed = 5.0f;

        if (length > min_length) {
            float speed = lerp(min_speed, max_speed, std::min(length, max_length) / max_length);
            const glm::vec approach_pos = glm::mix(from_pos, to_pos, speed * dt);
            engine::move_camera(engine, (int32_t)floorf(approach_pos.x), (int32_t)floor(approach_pos.y));
        }
    }
}

void game_state_playing_render(engine::Engine &engine, Game &game) {
    (void)engine;
    (void)game;
}

} // namespace game
