#include "game.h"
#include "action_binds.h"
#include <cassert>
#include <engine/engine.h>
#include <engine/input.h>
#include <engine/sprites.h>
#include <imgui.h>
#include <proto/game.pb.h>
#include <limits>
#include <GLFW/glfw3.h>

namespace game {

void game_state_playing_on_input(engine::Engine &engine, Game &game, engine::InputCommand &input_command) {
    assert(game.action_binds != nullptr);

    if (input_command.input_type == engine::InputType::Mouse) {
        if (input_command.mouse_state.mouse_action == engine::MouseAction::MouseMoved &&
            input_command.mouse_state.mouse_left_state == engine::TriggerState::Pressed) {
            int32_t x = input_command.mouse_state.mouse_relative_motion.x;
            int32_t y = input_command.mouse_state.mouse_relative_motion.y;
            engine::offset_camera(engine, x, -y);
        }
    }

    if (input_command.input_type == engine::InputType::Key) {
        bool pressed = input_command.key_state.trigger_state == engine::TriggerState::Pressed;
        bool released = input_command.key_state.trigger_state == engine::TriggerState::Released;
        bool repeated = input_command.key_state.trigger_state == engine::TriggerState::Repeated;

        ActionBindEntry_Action action = action_for_bind(*game.action_binds, (ActionBindEntry_Bind)input_command.key_state.keycode);
        switch(action) {
            case ActionBindEntry::QUIT: {
                if (pressed) {
                    transition(engine, &game, GameState::Quitting);
                }
                break;
            }
            case ActionBindEntry::DEBUG_HUD: {
                if (pressed) {
                    game.present_hud = !game.present_hud;
                }
                break;
            }
            default: break;
        }
    }
}

void game_state_playing_update(engine::Engine &engine, Game &game, float t, float dt) {
    (void)engine;
    (void)game;
    (void)t;
    (void)dt;
}

void game_state_playing_render(engine::Engine &engine, Game &game) {
    (void)engine;
    
    if (game.present_hud) {
    }
}

}
