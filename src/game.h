#pragma once

#include <collection_types.h>
#include <glm/fwd.hpp>

namespace std {
class thread;
class mutex;
} // namespace std

namespace engine {
struct Engine;
struct InputCommand;
struct Atlas;
struct Sprite;
} // namespace engine

namespace game {
using namespace foundation;

class GameParams;
class ActionBinds;
struct Level;
enum ActionBindEntry_Action : int;

struct Mob {
    uint64_t sprite_id;
    uint32_t index; // Tile index
    float energy;
};

/**
 * @brief An enum that describes a specific game state.
 * 
 */
enum class GameState {
    // No game state.
    None,

    // Game state is creating, or loading from a save.
    Initializing,

    // In menus
    Menus,

    // Generating a dungeon
    Dungen,

    // Playing the game.
    Playing,

    // Shutting down, saving and closing the game.
    Quitting,

    // Final state that signals the engine to terminate the application.
    Terminate,
};

// The game state.
struct Game {
    Game(Allocator &allocator);
    ~Game();

    Allocator &allocator;
    GameParams *params;
    ActionBinds *action_binds;
    GameState game_state;
    Level *level;

    Mob player_mob;

    std::mutex *dungen_mutex;
    std::thread *dungen_thread;

    bool present_hud;
    bool processing_turn;
    bool camera_locked_on_player;
    
    // These animations catch up mobs and synchronized effects. We wait until these complete before allowing player input.
    Hash<bool> processing_animations;

    ActionBindEntry_Action queued_action;
};

/**
 * @brief Updates the game
 *
 * @param engine The engine which calls this function
 * @param game_object The game to update
 * @param t The current time
 * @param dt The delta time since last update
 */
void update(engine::Engine &engine, void *game_object, float t, float dt);

/**
 * @brief Callback to the game that an input has ocurred.
 * 
 * @param engine The engine which calls this function
 * @param game_object The game to signal.
 * @param input_command The input command.
 */
void on_input(engine::Engine &engine, void *game_object, engine::InputCommand &input_command);

/**
 * @brief Renders the game
 *
 * @param engine The engine which calls this function
 * @param game_object The game to render
 */
void render(engine::Engine &engine, void *game_object);

/**
 * @brief Renders the imgui
 * 
 * @param engine The engine which calls this function
 * @param game_object The game to render
 */
void render_imgui(engine::Engine &engine, void *game_object);

/**
 * @brief Asks the game to quit.
 * 
 * @param engine The engine which calls this function
 * @param game_object The game to render
 */
void on_shutdown(engine::Engine &engine, void *game_object);

/**
 * @brief Transition a Game to another game state.
 * 
 * @param engine The engine which calls this function
 * @param game_object The game to transition
 * @param game_state The GameState to transition to.
 */
void transition(engine::Engine &engine, void *game_object, GameState game_state);

} // namespace game
