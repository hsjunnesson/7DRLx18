#pragma once

#include <collection_types.h>

namespace engine {
struct Engine;
}

namespace game {
using namespace foundation;

struct Game;

/**
 * @brief A room in the dungeon.
 * 
 */
struct Room {
    int32_t room_index;
    int32_t x, y;
    int32_t w, h;
    bool start_room = false;
    bool boss_room = false;
};

/**
 * @brief A corridor between rooms
 * 
 */
struct Corridor {
    int32_t from_room_index;
    int32_t to_room_index;
};

/**
 * @brief A tile in the dungeon.
 * 
 */
enum class Tile {
    None,
    Missing,
    Floor,
    Wall,
    WallCornerTopLeft,
    WallCornerTopRight,
    WallCornerBottomLeft,
    WallCornerBottomRight,
    WallTop,
    WallBottom,
    WallLeft,
    WallRight,
    WallLeftRight,
    WallTopBottom,
    WallCapRight,
    WallCapTop,
    WallCapLeft,
    WallCapBottom,
    CorridorCornerUpRight,
    CorridorCornerUpLeft,
    CorridorCornerDownRight,
    CorridorCornerDownLeft,
    StairsDown,
    StairsUp,
};

/// Returns the name of the sprite of the tile.
constexpr const char *tile_sprite_name(const Tile tile) {
    switch (tile) {
    case Tile::None:
        return "none";
    case Tile::Missing:
        return "missing";
    case Tile::Floor:
        return "floor";
    case Tile::Wall:
    case Tile::WallCornerTopLeft:
    case Tile::WallCornerTopRight:
    case Tile::WallCornerBottomLeft:
    case Tile::WallCornerBottomRight:
    case Tile::WallTop:
    case Tile::WallBottom:
    case Tile::WallLeft:
    case Tile::WallRight:
    case Tile::WallLeftRight:
    case Tile::WallTopBottom:
    case Tile::WallCapRight:
    case Tile::WallCapTop:
    case Tile::WallCapLeft:
    case Tile::WallCapBottom:
    case Tile::CorridorCornerUpRight:
    case Tile::CorridorCornerUpLeft:
    case Tile::CorridorCornerDownRight:
    case Tile::CorridorCornerDownLeft:
        return "wall";
    case Tile::StairsDown:
        return "stairs_down";
    case Tile::StairsUp:
        return "stairs_up";
    }
}

/**
 * @brief A level in the dungeon.
 * 
 */
struct Level {
    Level(Allocator &allocator);

    Hash<Room> rooms;
    Hash<Tile> tiles;
    Hash<uint64_t> tiles_sprite_ids;
    int32_t max_width;
    int32_t depth;
    int32_t stairs_up_pos;
    int32_t stairs_down_pos;
};

/**
 * @brief Dungeon generation thread function
 * 
 * @param engine The Engine to pass in
 * @param game The Game to pass in
 */
void dungen(engine::Engine *engine, game::Game *game);

} // namespace game
