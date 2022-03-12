#pragma once

#pragma warning(push, 0)
#include <collection_types.h>
#include <engine/math.inl>
#pragma warning(pop)

namespace engine {
struct Engine;
}

namespace game {
struct Game;
struct Room;

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
    FloorGrass,
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
        return "floor";
    case Tile::Floor:
        return "floor";
    case Tile::FloorGrass:
        return "floor_grass";
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
    case Tile::Missing:
        return "missing";
    default:
        return "missing";
    }
}

/**
 * @brief A level in the dungeon.
 * 
 */
struct Level {
    Level(foundation::Allocator &allocator);

    foundation::Hash<Room> rooms;
    foundation::Hash<Tile> tiles;
    foundation::Hash<uint64_t> tiles_sprite_ids;
    int32_t max_width;
    int32_t depth;
    int32_t stairs_up_index;
    int32_t stairs_down_index;
};

/**
 * @brief Dungeon generation thread function
 * 
 */
void dungen(engine::Engine *engine, game::Game *game);

} // namespace game
