#pragma once

#pragma warning(push, 0)
#include <collection_types.h>
#include <engine/math.inl>
#pragma warning(pop)

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
    Room() {
    }

    Room(int32_t room_index, int32_t room_template_index, int32_t x, int32_t y, int32_t w, int32_t h, bool start_room = false, bool boss_room = false)
    : room_index(room_index)
    , room_template_index(room_template_index)
    , x(x)
    , y(y)
    , w(w)
    , h(h)
    , start_room(start_room)
    , boss_room(boss_room) {}

    int32_t room_index;
    int32_t room_template_index;
    int32_t x, y;
    int32_t w, h;
    bool start_room = false;
    bool boss_room = false;
};

/**
 * @brief A collection of room templates.
 * 
 */
struct RoomTemplates {
    struct Template {
        enum class TileType: uint8_t {
            Empty = 0,
            Floor,
            Wall,
            Connection,
            Stair,
            Count
        };

        enum Tags {
            RoomTemplateTagsNone         = 0,
            RoomTemplateTagsStartRoom    = 1 << 0,
            RoomTemplateTagsBossRoom     = 1 << 1,
        };

        Template(Allocator &allocator);
        Template(const Template &other);
        ~Template();

        Allocator &allocator;
        Array<char> *name;
        uint8_t rarity;
        uint8_t tags;
        uint8_t rows;
        uint8_t columns;
        Array<uint8_t> *tiles;
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
    Level(Allocator &allocator);

    Hash<Room> rooms;
    Hash<Tile> tiles;
    Hash<uint64_t> tiles_sprite_ids;
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
