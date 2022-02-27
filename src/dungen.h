#pragma once

#include <collection_types.h>
#include <engine/math.inl>

namespace engine {
struct Engine;
}

namespace game {
using namespace foundation;

struct Game;

/**
 * @brief A room placed in the dungeon.
 * 
 */
struct Room {
    uint32_t room_index;
    uint32_t x, y;
    uint32_t w, h;
    bool start_room = false;
    bool boss_room = false;
};

/**
 * @brief A collection of room templates.
 * 
 */
struct RoomTemplates {
    struct Template {
        Template(Allocator &allocator);
        Template(const Template &other);
        ~Template();

        Allocator &allocator;
        Array<char> *name;
        uint8_t rows;
        uint8_t columns;
        Array<uint8_t> *data;
    };

    RoomTemplates(Allocator &allocator);
    ~RoomTemplates();
    
    Allocator &allocator;
    Array<Template *> templates;

    void read(const char *filename);
    void write(const char *filename);
};

/**
 * @brief A corridor between rooms
 * 
 */
struct Corridor {
    uint32_t from_room_index;
    uint32_t to_room_index;
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
    Level(Allocator &allocator);

    Hash<Room> rooms;
    Hash<Tile> tiles;
    Hash<uint64_t> tiles_sprite_ids;
    uint32_t max_width;
    uint32_t depth;
    uint32_t stairs_up_index;
    uint32_t stairs_down_index;
};

/**
 * @brief Dungeon generation thread function
 * 
 */
void dungen(engine::Engine *engine, game::Game *game);

} // namespace game
