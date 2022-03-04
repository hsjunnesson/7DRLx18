#include "game.h"
#include "action_binds.h"
#include "color.inl"
#include "dungen.h"
#include "editor.h"

#pragma warning(push, 0)
#include <engine/atlas.h>
#include <engine/config.inl>
#include <engine/engine.h>
#include <engine/input.h>
#include <engine/log.h>
#include <engine/math.inl>
#include <engine/sprites.h>
#include <proto/game.pb.h>

#include <array.h>
#include <hash.h>
#include <memory.h>
#include <temp_allocator.h>

#include <imgui.h>

#include <mutex>
#include <thread>
#pragma warning(pop)

namespace game {
using namespace math;

void game_state_playing_enter(engine::Engine &engine, Game &game);
void game_state_playing_leave(engine::Engine &engine, Game &game);
void game_state_playing_on_input(engine::Engine &engine, Game &game, engine::InputCommand &input_command);
void game_state_playing_update(engine::Engine &engine, Game &game, float t, float dt);
void game_state_playing_render(engine::Engine &engine, Game &game);

Game::Game(Allocator &allocator)
: allocator(allocator)
, params(nullptr)
, action_binds(nullptr)
, game_state(GameState::None)
, level(nullptr)
, room_templates(nullptr)
, player_mob(nullptr)
, dungen_mutex(nullptr)
, dungen_thread(nullptr)
, dungen_done(nullptr)
, presenting_imgui_demo(false)
, presenting_editor(false)
, editor_state(nullptr)
, processing_turn(false)
, camera_locked_on_player(false)
, processing_animations(allocator)
, queued_action(ActionBindEntry_Action_ACTION_UNKNOWN) {
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

    // dungen
    {
        dungen_mutex = MAKE_NEW(allocator, std::mutex);
        dungen_done = false;
    }

    // room templates
    {
        room_templates = MAKE_NEW(allocator, RoomTemplates, allocator);
    }

    // editor
    {
        editor_state = MAKE_NEW(allocator, editor::EditorState, allocator);
    }
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

    if (room_templates) {
        MAKE_DELETE(allocator, RoomTemplates, room_templates);
    }

    if (dungen_thread) {
        std::scoped_lock lock(*dungen_mutex);
        MAKE_DELETE(allocator, thread, dungen_thread);
    }

    if (dungen_mutex) {
        MAKE_DELETE(allocator, mutex, dungen_mutex);
    }

    if (editor_state) {
        MAKE_DELETE(allocator, EditorState, editor_state);
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
        std::scoped_lock lock(*game->dungen_mutex);
        if (game->dungen_done) {
            game::transition(engine, game_object, GameState::Playing);
        }
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

    if ((game->presenting_imgui_demo || game->presenting_editor) && input_command.input_type == engine::InputType::Mouse) {
        ImGuiIO &io = ImGui::GetIO();
        if (io.WantCaptureMouse) {
            return;
        }
    }

    if ((game->presenting_imgui_demo || game->presenting_editor) && input_command.input_type == engine::InputType::Scroll) {
        ImGuiIO &io = ImGui::GetIO();
        if (io.WantCaptureMouse) {
            return;
        }
    }

    if ((game->presenting_imgui_demo || game->presenting_editor) && input_command.input_type == engine::InputType::Key) {
        ImGuiIO &io = ImGui::GetIO();
        if (io.WantCaptureKeyboard) {
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

    if (game->presenting_imgui_demo) {
        ImGui::ShowDemoWindow();
    }

    if (game->presenting_editor) {
        editor::render_imgui(engine, *game, *game->editor_state);
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
    case GameState::Playing: {
        game_state_playing_leave(engine, *game);
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
        game->room_templates->read(game->params->room_templates_filename().c_str());

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
        game->dungen_done = false;
        game->dungen_thread = MAKE_NEW(game->allocator, std::thread, game::dungen, &engine, game);
        game->dungen_thread->detach();
        break;
    }
    case GameState::Playing: {
        log_info("Playing");
        game_state_playing_enter(engine, *game);
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
