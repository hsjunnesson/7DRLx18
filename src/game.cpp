#include "game.h"
#include "action_binds.h"

#include <engine/atlas.h>
#include <engine/config.inl>
#include <engine/engine.h>
#include <engine/input.h>
#include <engine/log.h>
#include <engine/sprites.h>
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

namespace game {

void game_state_playing_on_input(engine::Engine &engine, Game &game, engine::InputCommand &input_command);
void game_state_playing_update(engine::Engine &engine, Game &game, float t, float dt);
void game_state_playing_render(engine::Engine &engine, Game &game);

Game::Game(Allocator &allocator)
: allocator(allocator)
, params(nullptr)
, action_binds(nullptr)
, game_state(GameState::None)
, background_sprites(nullptr)
, mapgen_mutex(nullptr)
, mapgen_thread(nullptr)
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

    // background sprites
    {
        background_sprites = MAKE_NEW(allocator, foundation::Array<engine::Sprite *>, allocator);
    }

    mapgen_mutex = MAKE_NEW(allocator, std::mutex);
}

Game::~Game() {
    if (params) {
        MAKE_DELETE(allocator, GameParams, params);
    }

    if (action_binds) {
        MAKE_DELETE(allocator, ActionBinds, action_binds);
    }

    if (background_sprites) {
        MAKE_DELETE(allocator, Array, background_sprites);
    }

    if (mapgen_thread) {
        std::scoped_lock lock(*mapgen_mutex);
        MAKE_DELETE(allocator, thread, mapgen_thread);
    }

    if (mapgen_mutex) {
        MAKE_DELETE(allocator, mutex, mapgen_mutex);
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
    case GameState::Mapgen: {
        break;
    }
    case GameState::Playing: {
//        game_state_playing_update(engine, *game, t, dt);
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
            // game_state_playing_on_input(engine, *game, input_command);
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
    std::scoped_lock lock(*game->mapgen_mutex);

    switch (game->game_state) {
    case GameState::Menus: {
        break;
    }
    case GameState::Playing: {
        // game_state_playing_render(engine, *game);
        break;
    }
    default:
        break;
    }
}

void render_imgui(engine::Engine &engine, void *game_object) {
    (void)engine;

    Game *game = (Game *)game_object;
    std::scoped_lock lock(*game->mapgen_mutex);

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
    case GameState::Mapgen: {
        if (game->mapgen_thread) {
            MAKE_DELETE(game->allocator, thread, game->mapgen_thread);
            game->mapgen_thread = nullptr;
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
        transition(engine, game_object, GameState::Mapgen);

        break;
    }
    case GameState::Mapgen: {
        // game->mapgen_thread = MAKE_NEW(game->allocator, std::thread, game::mapgen, &engine, game, "foo");
        // game->mapgen_thread->detach();
        break;
    }
    case GameState::Playing: {
        log_info("Playing");

//		engine::move_camera(engine, -engine.window_rect.size.x / 2, -engine.window_rect.size.y / 2);

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
