#include "action_binds.h"
#include "array.h"
#include "game.h"
#include <GLFW/glfw3.h>
#include <cassert>
#include <engine/engine.h>
#include <engine/input.h>
#include <engine/sprites.h>
#include <imgui.h>
#include <limits>
#include <proto/game.pb.h>
#include <engine/log.h>

namespace game {

void player_move(int32_t x, int32_t y) {

}

void player_wait() {

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
}

void game_state_playing_render(engine::Engine &engine, Game &game) {
    (void)engine;

    if (game.present_hud) {
    }
}

} // namespace game
