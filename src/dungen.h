#pragma once

namespace engine {
struct Engine;
}

namespace game {
struct Game;

/**
 * @brief Dungeon generation thread function
 * 
 * @param engine The Engine to pass in
 * @param game The Game to pass in
 * @param seed The seed
 */
void dungen(engine::Engine *engine, game::Game *game, const char *seed);

} // namespace game
