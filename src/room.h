#pragma once

#pragma warning(push, 0)
#include <collection_types.h>
#include <stdint.h>
#pragma warning(pop)

namespace game {

/**
 * @brief A room placed in the dungeon.
 * 
 */
struct Room {
    Room() {};

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
struct RoomTemplate {
    enum class TileType : uint8_t {
        Empty = 0,
        Floor,
        Wall,
        Connection,
        Stair,
        Count
    };

    enum Tags {
        RoomTemplateTagsNone = 0,
        RoomTemplateTagsStartRoom = 1 << 0,
        RoomTemplateTagsBossRoom = 1 << 1,
    };

    explicit RoomTemplate(foundation::Allocator &allocator);
    explicit RoomTemplate(const RoomTemplate &other);
    RoomTemplate(RoomTemplate &&) noexcept = delete;
    RoomTemplate &operator=(const RoomTemplate &) = delete;
    RoomTemplate &operator=(RoomTemplate &&) noexcept = delete;
    ~RoomTemplate();

    foundation::Allocator &allocator;
    foundation::Array<char> *name;
    uint8_t rarity;
    uint8_t tags;
    uint8_t rows;
    uint8_t columns;
    foundation::Array<uint8_t> *tiles;
};

struct RoomTemplates {
    RoomTemplates(foundation::Allocator &allocator);
    ~RoomTemplates();
    RoomTemplates(const RoomTemplates &) = delete;
    RoomTemplates &operator=(const RoomTemplates &) = delete;

    foundation::Allocator &allocator;
    foundation::Array<RoomTemplate *> room_templates;

    void read(const char *filename);
    void write(const char *filename);
};

} // namespace game
