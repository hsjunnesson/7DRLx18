#include "game.h"
#include "action_binds.h"
#include "dungen.h"
#include "color.inl"

#include <engine/atlas.h>
#include <engine/config.inl>
#include <engine/engine.h>
#include <engine/input.h>
#include <engine/log.h>
#include <engine/sprites.h>
#include <engine/math.inl>
#include <proto/game.pb.h>

#include <array.h>
#include <hash.h>
#include <memory.h>
#include <temp_allocator.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <imgui.h>

#include <mutex>
#include <thread>

namespace {
const float LEVEL_Z_LAYER = -5.0f;
const float ITEM_Z_LAYER = -4.0f;
const float MOB_Z_LAYER = -3.0f;
} // namespace

namespace game {
using namespace math;

void game_state_playing_on_input(engine::Engine &engine, Game &game, engine::InputCommand &input_command);
void game_state_playing_update(engine::Engine &engine, Game &game, float t, float dt);
void game_state_playing_render(engine::Engine &engine, Game &game);

/// Return the world coordinate of the center of a tile at pos, scaled by zoom and render scale.
const glm::vec2 pos_to_world(const engine::Engine &engine, const Game &game, const uint32_t pos) {
    uint32_t x, y;
    coord(pos, x, y, game.level->max_width);
    
    glm::vec2 w {x, y};
    w *= game.params->tilesize() * engine.camera_zoom * engine.render_scale;
    w += game.params->tilesize();

    return w;
}

/// Centers the view on the tile at pos.
void center_view_to_pos(engine::Engine &engine, const Game &game, const uint32_t pos) {
    glm::vec2 w = pos_to_world(engine, game, pos);
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

Game::Game(Allocator &allocator)
: allocator(allocator)
, params(nullptr)
, action_binds(nullptr)
, game_state(GameState::None)
, level(nullptr)
, player_pos(0)
, player_sprite_id(0)
, dungen_mutex(nullptr)
, dungen_thread(nullptr)
, present_hud(false) {
    // Default assets paths
    const char *params_path = "assets/game_params.json";
    const char *action_binds_path = "assets/action_binds.json";

    params = MAKE_NEW(allocator, GameParams);
    engine::config::read(params_path, params);

    // action binds
    {
        action_binds = MAKE_NEW(allocator, ActionBinds);
        engine::config::read(action_binds_path, action_binds);
        if (!validate_actionbinds(*action_binds)) {
            log_fatal("Invalid action binds");
        }
    }

    // level
    {
        level = MAKE_NEW(allocator, Level, allocator);
    }

    dungen_mutex = MAKE_NEW(allocator, std::mutex);
}

Game::~Game() {
    if (params) {
        MAKE_DELETE(allocator, GameParams, params);
    }

    if (action_binds) {
        MAKE_DELETE(allocator, ActionBinds, action_binds);
    }

    if (level) {
        MAKE_DELETE(allocator, Level, level);
    }

    if (dungen_thread) {
        std::scoped_lock lock(*dungen_mutex);
        MAKE_DELETE(allocator, thread, dungen_thread);
    }

    if (dungen_mutex) {
        MAKE_DELETE(allocator, mutex, dungen_mutex);
    }
}

void update(engine::Engine &engine, void *game_object, float t, float dt) {
    if (!game_object) {
        return;
    }

    Game *game = (Game *)game_object;

    switch (game->game_state) {
    case GameState::None: {
        transition(engine, game_object, GameState::Initializing);
        break;
    }
    case GameState::Dungen: {
        break;
    }
    case GameState::Playing: {
        game_state_playing_update(engine, *game, t, dt);
        break;
    }
    case GameState::Quitting: {
        transition(engine, game_object, GameState::Terminate);
        break;
    }
    default: {
        break;
    }
    }
}

void on_input(engine::Engine &engine, void *game_object, engine::InputCommand &input_command) {
    if (!game_object) {
        return;
    }

    Game *game = (Game *)game_object;

    if (game->present_hud && input_command.input_type == engine::InputType::Mouse) {
        ImGuiIO &io = ImGui::GetIO();
        if (io.WantCaptureMouse) {
            return;
        }
    }

    switch (game->game_state) {
    case GameState::Playing: {
        game_state_playing_on_input(engine, *game, input_command);
        break;
    }
    default: {
        break;
    }
    }
}

void render(engine::Engine &engine, void *game_object) {
    (void)engine;

    if (!game_object) {
        return;
    }

    Game *game = (Game *)game_object;
    std::scoped_lock lock(*game->dungen_mutex);

    switch (game->game_state) {
    case GameState::Menus: {
        break;
    }
    case GameState::Playing: {
        game_state_playing_render(engine, *game);
        break;
    }
    default:
        break;
    }
}

void render_imgui(engine::Engine &engine, void *game_object) {
    (void)engine;

    Game *game = (Game *)game_object;
    std::scoped_lock lock(*game->dungen_mutex);

    TempAllocator128 ta;

    if (game->present_hud) {
    }
}

void on_shutdown(engine::Engine &engine, void *game_object) {
    transition(engine, game_object, GameState::Quitting);
}

void transition(engine::Engine &engine, void *game_object, GameState game_state) {
    if (!game_object) {
        return;
    }

    Game *game = (Game *)game_object;

    if (game->game_state == game_state) {
        return;
    }

    // When leaving a game state
    switch (game->game_state) {
    case GameState::Dungen: {
        if (game->dungen_thread) {
            MAKE_DELETE(game->allocator, thread, game->dungen_thread);
            game->dungen_thread = nullptr;
        }
        break;
    }
    case GameState::Terminate: {
        return;
    }
    default:
        break;
    }

    game->game_state = game_state;

    // When entering a new game state
    switch (game->game_state) {
    case GameState::None: {
        break;
    }
    case GameState::Initializing: {
        log_info("Initializing");
        engine::init_sprites(*engine.sprites, game->params->game_atlas_filename().c_str());
        transition(engine, game_object, GameState::Menus);
        break;
    }
    case GameState::Menus: {
        log_info("Menus");
        transition(engine, game_object, GameState::Dungen);
        break;
    }
    case GameState::Dungen: {
        log_info("Dungen");
        game->dungen_thread = MAKE_NEW(game->allocator, std::thread, game::dungen, &engine, game);
        game->dungen_thread->detach();
        break;
    }
    case GameState::Playing: {
        log_info("Playing");

        // Create level tiles
        {
            hash::clear(game->level->tiles_sprite_ids);

            for (auto iter = hash::begin(game->level->tiles); iter != hash::end(game->level->tiles); ++iter) {
                uint64_t pos = iter->key;

                Tile tile = static_cast<Tile>(iter->value);
                const char *sprite_name = tile_sprite_name(tile);
                uint64_t sprite_id = add_sprite(*engine.sprites, sprite_name, game->params->tilesize(), pos, game->level->max_width, LEVEL_Z_LAYER);
                hash::set(game->level->tiles_sprite_ids, pos, sprite_id);
            }
        }

        // Create player
        {
            game->player_pos = game->level->stairs_up_pos;
            uint64_t sprite_id = add_sprite(*engine.sprites, "farmer", game->params->tilesize(), game->player_pos, game->level->max_width, MOB_Z_LAYER, color::peach);
            game->player_sprite_id = sprite_id;
            center_view_to_pos(engine, *game, game->player_pos);
        }

        break;
    }
    case GameState::Quitting: {
        log_info("Quitting");
        break;
    }
    case GameState::Terminate: {
        log_info("Terminating");
        engine::terminate(engine);
        break;
    }
    }
}

} // namespace game
