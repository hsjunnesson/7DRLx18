#include "dungen.h"
#include "game.h"
#include "line.hpp"
#include "room.h"
#include "mob.h"

#pragma warning(push, 0)
#include "rnd.h"

#include "engine/config.inl"
#include "engine/engine.h"
#include "engine/file.h"
#include "engine/log.h"
#include "engine/math.inl"
#include "engine/sprites.h"
#include "proto/game.pb.h"

#include <algorithm>
#include <array.h>
#include <hash.h>
#include <memory.h>
#include <murmur_hash.h>
#include <queue.h>
#include <string_stream.h>
#include <temp_allocator.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cassert>
#include <mutex>
#include <thread>
#pragma warning(pop)

namespace game {
using namespace foundation;
using engine::coord;
using engine::index;

enum class ConnectionDirection {
    North,
    East,
    South,
    West
};

void connections_from_direction(const RoomTemplate &room_template, ConnectionDirection direction, Array<int32_t> &coordinates) {
    TempAllocator128 ta;
    Hash<int32_t> side_data(ta);
    uint8_t empty_tile_type = static_cast<uint8_t>(RoomTemplate::TileType::Empty);
    uint8_t connection_tile_type = static_cast<uint8_t>(RoomTemplate::TileType::Connection);

    switch (direction) {
    case ConnectionDirection::North:
    case ConnectionDirection::South: {
        int32_t start_row = direction == ConnectionDirection::North ? 0 : room_template.rows - 1;
        int32_t end_row = direction == ConnectionDirection::North ? room_template.rows : 0;
        int32_t row_offset = direction == ConnectionDirection::North ? 1 : -1;

        for (int32_t column = 0; column < room_template.columns; ++column) {
            for (int32_t row = start_row; row != end_row; row += row_offset) {
                int32_t index = math::index(column, row, room_template.columns);
                uint8_t tile_type = (*room_template.tiles)[index];
                if (tile_type == empty_tile_type) {
                    continue;
                } else {
                    if (tile_type == connection_tile_type) {
                        hash::set(side_data, column, row);
                    }
                    break;
                }
            }
        }
        break;
    }
    case ConnectionDirection::West:
    case ConnectionDirection::East: {
        int32_t start_column = direction == ConnectionDirection::West ? 0 : room_template.columns - 1;
        int32_t end_column = direction == ConnectionDirection::West ? room_template.columns : 0;
        int32_t column_offset = direction == ConnectionDirection::West ? 1 : -1;

        for (int32_t row = 0; row < room_template.rows; ++row) {
            for (int32_t column = start_column; column != end_column; column += column_offset) {
                int32_t index = math::index(column, row, room_template.columns);
                uint8_t tile_type = (*room_template.tiles)[index];
                if (tile_type == empty_tile_type) {
                    continue;
                } else {
                    if (tile_type == connection_tile_type) {
                        hash::set(side_data, row, column);
                    }
                    break;
                }
            }
        }
        break;
    }
    }

    for (auto iter = hash::begin(side_data); iter != hash::end(side_data); ++iter) {
        int32_t index = 0;

        switch (direction) {
        case ConnectionDirection::North:
        case ConnectionDirection::South: {
            int32_t column = (int32_t)iter->key;
            int32_t row = iter->value;
            index = math::index(column, row, room_template.columns);
            break;
        }
        case ConnectionDirection::East:
        case ConnectionDirection::West: {
            int32_t row = (int32_t)iter->key;
            int32_t column = iter->value;
            index = math::index(column, row, room_template.columns);
            break;
        }
        }

        array::push_back(coordinates, index);
    }
}

Level::Level(Allocator &allocator)
: rooms(allocator)
, tiles(allocator)
, tiles_sprite_ids(allocator)
, max_width(0)
, depth(0)
, stairs_up_index(0)
, stairs_down_index(0) {}

void dungen(engine::Engine *engine, game::Game *game) {
    assert(engine);
    assert(game);

    TempAllocator1024 ta;

    DungenParams params;
    engine::config::read(game->params->dungen_params_filename().c_str(), &params);

    Hash<Tile> terrain_tiles = Hash<Tile>(ta);

    const int32_t map_width = params.map_width();
    const int32_t room_count = params.room_count();
    const int32_t rooms_count_wide = (int32_t)ceil(sqrt(room_count));
    const int32_t rooms_count_tall = (int32_t)ceil(sqrt(room_count));
    const int32_t section_width = map_width / rooms_count_wide;
    const int32_t section_height = map_width / rooms_count_tall;
    const int32_t max_room_spawns = 4;

    log_debug("Dungen rooms count %u", room_count);
    log_debug("Dungen rooms count wide %u", rooms_count_wide);
    log_debug("Dungen rooms count tall %u", rooms_count_tall);
    log_debug("Dungen section width %u", section_width);
    log_debug("Dungen section height %u", section_height);

    if (floor(sqrt(room_count)) != ceil(sqrt(room_count))) {
        log_fatal("room_count parameter not a square.");
    }

    rnd_pcg_t random_device;
    unsigned int seed = (unsigned int)time(nullptr);

    rnd_pcg_seed(&random_device, seed);

    log_debug("Dungen seeded with %u", seed);

    int32_t start_room_index = 0;
    int32_t boss_room_index = 0;

    int32_t stairs_up_index = 0;
    int32_t stairs_down_index = 0;

    // Collections of room templates
    Array<int32_t> common_room_template_indices(ta);
    Array<int32_t> start_room_template_indices(ta);
    Array<int32_t> boss_room_template_indices(ta);

    // Multi hash of room indices by rarity.
    Hash<int32_t> common_room_indices_by_rarity(ta);

    {
        for (int32_t i = 0; i < (int32_t)array::size(game->room_templates->room_templates); ++i) {
            RoomTemplate *room_template = game->room_templates->room_templates[i];
            bool common_room = true;
            uint8_t tags = room_template->tags;

            if ((tags & RoomTemplate::RoomTemplateTagsStartRoom) != 0) {
                array::push_back(start_room_template_indices, i);
                common_room = false;
            }

            if ((tags & RoomTemplate::RoomTemplateTagsBossRoom) != 0) {
                array::push_back(boss_room_template_indices, i);
                common_room = false;
            }

            if (common_room) {
                array::push_back(common_room_template_indices, i);
                multi_hash::insert(common_room_indices_by_rarity, room_template->rarity, i);
            }
        }
    }

    // Rooms and corridors collections
    Hash<Room> rooms = Hash<Room>(ta);
    Array<Corridor> corridors = Array<Corridor>(ta);

    // Decide whether main orientation is vertical or horizontal
    {
        bool start_room_vertical_side = rnd_pcg_range(&random_device, 0, 1) == 0;
        bool start_room_first_side = rnd_pcg_range(&random_device, 0, 1) == 0;

        if (start_room_vertical_side) {
            int32_t y = rnd_pcg_range(&random_device, 0, rooms_count_tall - 1);
            start_room_index = y * rooms_count_wide;

            if (!start_room_first_side) {
                start_room_index += (rooms_count_wide - 1);
            }

            y = rnd_pcg_range(&random_device, 0, rooms_count_tall - 1);
            boss_room_index = y * rooms_count_wide;

            if (start_room_first_side) {
                boss_room_index += (rooms_count_wide - 1);
            }
        } else {
            int32_t x = rnd_pcg_range(&random_device, 0, rooms_count_wide - 1);
            start_room_index = x;

            if (!start_room_first_side) {
                start_room_index += rooms_count_wide * (rooms_count_tall - 1);
            }

            x = rnd_pcg_range(&random_device, 0, rooms_count_wide - 1);
            boss_room_index = x;

            if (start_room_first_side) {
                start_room_index += rooms_count_wide * (rooms_count_tall - 1);
            }
        }

        if (start_room_index > room_count) {
            start_room_index = room_count - 1;
        }

        if (boss_room_index > room_count) {
            boss_room_index = room_count - 1;
        }

        log_debug("Dungen start room index %d", start_room_index);
        log_debug("Dungen boss room index %d", boss_room_index);
    }

    // Place rooms in grids sections, referenced by their index.
    {
        Array<int32_t> indices(ta);

        // Validate rarities
        bool valid_rarities = true;
        // TODO: Find max rarity from templates
        const int32_t max_rarity = 4;

        for (int32_t i = 1; i <= max_rarity; ++i) {
            if (!hash::has(common_room_indices_by_rarity, i)) {
                valid_rarities = false;
                log_error("Room template not in does not have room templates of rarity %u", i);
                break;
            }
        }

        for (int32_t room_index = 0; room_index < room_count; ++room_index) {
            int32_t room_index_x, room_index_y;
            coord(room_index, room_index_x, room_index_y, rooms_count_wide);

            const int32_t section_min_x = room_index_x * section_width;
            const int32_t section_max_x = section_min_x + section_width;
            const int32_t section_min_y = room_index_y * section_height;
            const int32_t section_max_y = section_min_y + section_height;

            int32_t room_template_index = 0;
            if (room_index == start_room_index) {
                room_template_index = start_room_template_indices[rnd_pcg_range(&random_device, 0, array::size(start_room_template_indices) - 1)];
            } else if (room_index == boss_room_index) {
                room_template_index = boss_room_template_indices[rnd_pcg_range(&random_device, 0, array::size(boss_room_template_indices) - 1)];
            } else {
                // Random rarity
                // Rarity is assumed to be either 1, 2, 3, 4
                // pow(i, 3) means distribution is in a d100:
                // 1: <= 57.8125 (57.8%)
                // 2: < 87.5 (29.68%)
                // 3: < 98.4375 (10.93%)
                // 4: < 100.0 (1.56%)

                const float power = 3.0f;
                uint8_t rarity = 1;

                if (valid_rarities) {
                    float roll = rnd_pcg_nextf(&random_device);
                    float chance = (1.0f - (powf(max_rarity - 1.0f, power) / powf(max_rarity, power)));
                    if (roll > chance) {
                        for (uint8_t i = 2; i <= max_rarity; ++i) {
                            chance = (1.0f - (powf(max_rarity - (float)i, power) / powf(max_rarity, power)));
                            if (roll <= chance) {
                                rarity = i;
                                break;
                            }
                        }
                    }
                }

                {
                    int32_t indices_count = multi_hash::count(common_room_indices_by_rarity, rarity);
                    int32_t random_index = rnd_pcg_range(&random_device, 0, indices_count - 1);

                    const foundation::Hash<int32_t>::Entry *entry = multi_hash::find_first(common_room_indices_by_rarity, rarity);

                    if (random_index > 0) {
                        int32_t count = 0;
                        while (count < random_index) {
                            entry = multi_hash::find_next(common_room_indices_by_rarity, entry);
                            ++count;
                        }
                    }

                    room_template_index = entry->value;
                }
            }

            const RoomTemplate &room_template = *game->room_templates->room_templates[room_template_index];

            // TODO: Randomize positions in section

            // const int32_t room_width = room_size_distribution(random_engine);
            // const int32_t room_height = room_size_distribution(random_engine);

            // std::uniform_int_distribution<int32_t> x_offset(section_min_x + 2, section_max_x - 2 - room_width);
            // std::uniform_int_distribution<int32_t> y_offset(section_min_y + 2, section_max_y - 2 - room_height);

            // const int32_t room_x = x_offset(random_engine);
            // const int32_t room_y = y_offset(random_engine);

            const int32_t room_width = room_template.columns;
            const int32_t room_height = room_template.rows;

            const int32_t room_x = section_min_x;
            const int32_t room_y = section_min_y;

            Room room(room_index, room_template_index, room_x, room_y, room_width, room_height);

            if (room_index == start_room_index) {
                room.start_room = true;
            }

            if (room_index == boss_room_index) {
                room.boss_room = true;
            }

            hash::set(rooms, room_index, room);
        }
    }

    // Place corridors
    {
        int32_t start_room_x, start_room_y;
        coord(start_room_index, start_room_x, start_room_y, rooms_count_wide);

        int32_t boss_room_x, boss_room_y;
        coord(boss_room_index, boss_room_x, boss_room_y, rooms_count_wide);

        Array<line::Coordinate> shortest_line_path = line::zig_zag(ta, {(int32_t)start_room_x, (int32_t)start_room_y}, {(int32_t)boss_room_x, (int32_t)boss_room_y});

        for (int32_t i = 0; i < (int32_t)array::size(shortest_line_path) - 1; ++i) {
            line::Coordinate from = shortest_line_path[i];
            line::Coordinate to = shortest_line_path[i + 1];

            int32_t from_room_index = index(from.x, from.y, rooms_count_wide);
            int32_t to_room_index = index(to.x, to.y, rooms_count_wide);

            array::push_back(corridors, Corridor{from_room_index, to_room_index});
        }
    }

    // Expand some branches
    {
        Array<Corridor> branches = Array<Corridor>(ta);

        // Returns a corridor to a random adjacent room, which isn't already connected to this room.
        auto expand = [&](int32_t room_index) {
            int32_t room_x, room_y;
            coord(room_index, room_x, room_y, rooms_count_wide);

            Array<int32_t> available_adjacent_room_indices = Array<int32_t>(ta);

            auto adjacent_coordinates = {
                line::Coordinate{(int32_t)room_x + 1, (int32_t)room_y},
                line::Coordinate{(int32_t)room_x, (int32_t)room_y + 1},
                line::Coordinate{(int32_t)room_x - 1, (int32_t)room_y},
                line::Coordinate{(int32_t)room_x, (int32_t)room_y - 1}};

            // For each orthogonally adjacent room. Check if it's a valid location.
            for (line::Coordinate next_coordinate : adjacent_coordinates) {
                if (next_coordinate.x >= 0 &&
                    next_coordinate.x < (int32_t)rooms_count_wide &&
                    next_coordinate.y >= 0 &&
                    next_coordinate.y < (int32_t)rooms_count_tall) {
                    int32_t next_room_index = index(next_coordinate.x, next_coordinate.y, rooms_count_wide);
                    bool found = false;

                    // Check to make sure no other corridor exists that connect these two rooms.
                    for (auto iter = array::begin(corridors); iter != array::end(corridors); ++iter) {
                        if ((iter->from_room_index == room_index && iter->to_room_index == next_room_index) ||
                            (iter->to_room_index == room_index && iter->from_room_index == next_room_index)) {
                            found = true;
                            break;
                        }
                    }

                    if (!found) {
                        array::push_back(available_adjacent_room_indices, next_room_index);
                    }
                }
            }

            if (array::size(available_adjacent_room_indices) > 0) {
                int32_t random_selection = rnd_pcg_range(&random_device, 0, array::size(available_adjacent_room_indices) - 1);
                int32_t next_room_index = available_adjacent_room_indices[random_selection];
                Corridor branching_corridor = Corridor{room_index, next_room_index};
                array::push_back(corridors, branching_corridor);
                return next_room_index;
            }

            return 0;
        };

        for (auto iter = array::begin(corridors); iter != array::end(corridors); ++iter) {
            // Don't branch from start or end room
            if (iter == array::begin(corridors)) {
                continue;
            }

            Corridor corridor = *iter;
            int32_t room_index = corridor.from_room_index;

            // Expand branches, perhaps multiple times
            bool expansion_done = rnd_pcg_range(&random_device, 0, 100) >= params.expand_chance();
            while (!expansion_done) {
                int32_t next_room_index = expand(room_index);
                if (next_room_index >= 0) {
                    room_index = next_room_index;
                    expansion_done = rnd_pcg_range(&random_device, 0, 100) >= params.expand_chance();
                } else {
                    expansion_done = true;
                }
            }
        }

        for (auto iter = array::begin(branches); iter != array::end(branches); ++iter) {
            Corridor branch = *iter;
            array::push_back(corridors, branch);
        }
    }

    // Prune disconnected rooms
    // TODO: Prune rooms that aren't connected to main path.
    // Clusters of connected rooms aren't pruned properly.
    {
        Hash<Room> saved_rooms = Hash<Room>(ta);

        for (int32_t i = 0; i < room_count; ++i) {
            bool found = false;

            for (auto iter = array::begin(corridors); iter != array::end(corridors); ++iter) {
                Corridor &corridor = *iter;
                if (corridor.from_room_index == i || corridor.to_room_index == i) {
                    found = true;
                    break;
                }
            }

            if (found) {
                Room room = hash::get(rooms, i, Room());
                hash::set(saved_rooms, i, room);
            }
        }

        rooms = saved_rooms;
    }

    // Move map to origin
    {
        int32_t min_x = INT32_MAX;
        int32_t min_y = INT32_MAX;

        for (auto iter = hash::begin(rooms); iter != hash::end(rooms); ++iter) {
            Room &room = iter->value;
            if (room.x < min_x) {
                min_x = room.x;
            }

            if (room.y < min_y) {
                min_y = room.y;
            }
        }

        // Add a border of one spaces.
        --min_x;
        --min_y;

        for (auto iter = hash::begin(rooms); iter != hash::end(rooms); ++iter) {
            Room &room = iter->value;
            room.x -= min_x;
            room.y -= min_y;
        }
    }

    // Print rooms
    {
        for (auto iter = hash::begin(rooms); iter != hash::end(rooms); ++iter) {
            const Room &room = iter->value;
            const RoomTemplate &room_template = *game->room_templates->room_templates[room.room_template_index];

            log_debug("Room %s %u,%u", string_stream::c_str(*room_template.name), room.x, room.y);
        }
    }

    // Draw rooms as tiles
    {
        for (auto iter = hash::begin(rooms); iter != hash::end(rooms); ++iter) {
            const Room &room = iter->value;
            const RoomTemplate &room_template = *game->room_templates->room_templates[room.room_template_index];

            for (uint32_t i = 0; i < array::size(*room_template.tiles); ++i) {
                Tile tile = Tile::None;
                RoomTemplate::TileType tile_type = static_cast<RoomTemplate::TileType>((*room_template.tiles)[i]);

                int32_t x, y;
                coord(i, x, y, room_template.columns);
                y = room_template.rows - y - 1;

                switch (tile_type) {
                case RoomTemplate::TileType::Empty:
                    tile = Tile::None;
                    break;
                case RoomTemplate::TileType::Floor:
                    tile = Tile::Floor;
                    break;
                case RoomTemplate::TileType::Wall:
                case RoomTemplate::TileType::Connection:
                    tile = Tile::Wall;
                    break;
                default:
                    tile = Tile::Missing;
                    break;
                }

                hash::set(terrain_tiles, index(room.x + x, room.y + y, map_width), {tile});
            }
        }
    }

    // Add stairs
    {
        const Tile stairs_down_tile = Tile::StairsDown;
        const Tile stairs_up_tile = Tile::StairsUp;

        Room start_room = hash::get(rooms, start_room_index, {});
        if (start_room.start_room) {
            const RoomTemplate &room_template = *game->room_templates->room_templates[start_room.room_template_index];
            for (uint32_t i = 0; i < array::size(*room_template.tiles); ++i) {
                uint8_t tile_type = (*room_template.tiles)[i];
                if (RoomTemplate::TileType::Stair == static_cast<RoomTemplate::TileType>(tile_type)) {
                    int32_t x, y;
                    coord(i, x, y, room_template.columns);
                    y = room_template.rows - y - 1;
                    stairs_up_index = index(start_room.x + x, start_room.y + y, map_width);
                    hash::set(terrain_tiles, stairs_up_index, {stairs_up_tile});
                    break;
                }
            }
        }

        Room boss_room = hash::get(rooms, boss_room_index, {});
        if (boss_room.boss_room) {
            const RoomTemplate *room_template = game->room_templates->room_templates[boss_room.room_template_index];
            for (uint32_t i = 0; i < array::size(*room_template->tiles); ++i) {
                uint8_t tile_type = (*room_template->tiles)[i];
                if (RoomTemplate::TileType::Stair == static_cast<RoomTemplate::TileType>(tile_type)) {
                    int32_t x, y;
                    coord(i, x, y, room_template->columns);
                    y = room_template->rows - y - 1;
                    stairs_down_index = index(boss_room.x + x, boss_room.y + y, map_width);
                    hash::set(terrain_tiles, stairs_down_index, {stairs_down_tile});
                    break;
                }
            }
        }
    }

    // Draw corridors as terrain_tiles
    {
        // Added walls which needs to be properly placed.
        Hash<bool> placeholder_walls = Hash<bool>(ta);

        int line_draw_count = 0;

        // Function to iterate through corridors, applying a function on each coordinate.
        // Note: These will randomize connection points for corridors, and since iterate_corridor is used multiple times
        // the random functions are seeded based on the rooms' indices.
        auto iterate_corridor = [&](std::function<void(line::Coordinate prev, line::Coordinate coord, line::Coordinate next)> apply) {
            for (Corridor *iter = array::begin(corridors); iter != array::end(corridors); ++iter) {
                Corridor corridor = *iter;

                const Room start_room = hash::get(rooms, corridor.from_room_index, {});
                const Room to_room = hash::get(rooms, corridor.to_room_index, {});
                const RoomTemplate &start_room_template = *game->room_templates->room_templates[start_room.room_template_index];
                const RoomTemplate &to_room_template = *game->room_templates->room_templates[to_room.room_template_index];

                line::Coordinate start_coordinate, to_coordinate;

                {
                    int32_t start_room_section_x, start_room_section_y;
                    coord(start_room.room_index, start_room_section_x, start_room_section_y, rooms_count_wide);

                    int32_t to_room_section_x, to_room_section_y;
                    coord(to_room.room_index, to_room_section_x, to_room_section_y, rooms_count_wide);

                    TempAllocator128 ta;

                    if (start_room_section_x != to_room_section_x && start_room_section_y != to_room_section_y) {
                        log_error("Rooms connected aren't orthogonally adjacent");
                        start_coordinate = {(int32_t)start_room.x + (int32_t)start_room.w / 2, (int32_t)start_room.y + (int32_t)start_room.h / 2};
                        to_coordinate = {(int32_t)to_room.x + (int32_t)to_room.w / 2, (int32_t)to_room.y + (int32_t)to_room.h / 2};
                    } else {
                        ConnectionDirection start_direction, to_direction;
                        if (start_room_section_y == to_room_section_y) { // horizontally adjacent
                            start_direction = start_room_section_x < to_room_section_x ? ConnectionDirection::East : ConnectionDirection::West;
                            to_direction = start_room_section_x < to_room_section_x ? ConnectionDirection::West : ConnectionDirection::East;
                        } else {
                            start_direction = start_room_section_y < to_room_section_y ? ConnectionDirection::North : ConnectionDirection::South;
                            to_direction = start_room_section_y < to_room_section_y ? ConnectionDirection::South : ConnectionDirection::North;
                        }

                        Array<int32_t> start_connections(ta);
                        connections_from_direction(start_room_template, start_direction, start_connections);
                        if (array::empty(start_connections)) {
                            log_fatal("Room template %s missing connections side %u", string_stream::c_str(*start_room_template.name), start_direction);
                        }

                        Array<int32_t> to_connections(ta);
                        connections_from_direction(to_room_template, to_direction, to_connections);
                        if (array::empty(to_connections)) {
                            log_fatal("Room template %s missing connections side %u", string_stream::c_str(*to_room_template.name), to_direction);
                        }

                        rnd_pcg_t pcg;
                        rnd_pcg_seed(&pcg, start_room.room_index);
                        int32_t start_connection_index = rnd_pcg_range(&pcg, 0, array::size(start_connections) - 1);

                        rnd_pcg_seed(&pcg, to_room.room_index);
                        int32_t to_connection_index = rnd_pcg_range(&pcg, 0, array::size(to_connections) - 1);

                        int32_t start_connection_x, start_connection_y;
                        coord(start_connections[start_connection_index], start_connection_x, start_connection_y, start_room_template.columns);
                        start_connection_y = start_room_template.rows - start_connection_y - 1;

                        int32_t to_connection_x, to_connection_y;
                        coord(to_connections[to_connection_index], to_connection_x, to_connection_y, to_room_template.columns);
                        to_connection_y = to_room_template.rows - to_connection_y - 1;

                        start_coordinate = {(int32_t)start_room.x + (int32_t)start_connection_x, (int32_t)start_room.y + start_connection_y};
                        to_coordinate = {(int32_t)to_room.x + (int32_t)to_connection_x, (int32_t)to_room.y + to_connection_y};
                    }
                }

                Array<line::Coordinate> coordinates = line::zig_zag(ta, start_coordinate, to_coordinate);

                for (int32_t line_i = 0; line_i < (int32_t)array::size(coordinates); ++line_i) {
                    line::Coordinate prev;
                    if (line_i > 0) {
                        prev = coordinates[line_i - 1];
                    } else {
                        prev.x = -1;
                        prev.y = -1;
                    }

                    line::Coordinate coord = coordinates[line_i];

                    line::Coordinate next;
                    if (line_i < (int32_t)array::size(coordinates) - 1) {
                        next = coordinates[line_i + 1];
                    } else {
                        next.x = -1;
                        next.y = -1;
                    }

                    apply(prev, coord, next);
                }

                ++line_draw_count;
            }
        };

        // Dig out corridors
        iterate_corridor([&](line::Coordinate prev, line::Coordinate coord, line::Coordinate next) {
            (void)prev;
            (void)next;
            hash::set(terrain_tiles, index(coord.x, coord.y, map_width), {Tile::Floor});
        });

        // Place placeholder walls
        iterate_corridor([&](line::Coordinate prev, line::Coordinate coord, line::Coordinate next) {
            // Valid next and prev
            if (prev.x >= 0 && next.x >= 0) {
                if (prev.x != next.x && prev.y == next.y) { // Horizontal line
                    assert(coord.y > 0);

                    int32_t above = index(coord.x, coord.y - 1, map_width);
                    int32_t below = index(coord.x, coord.y + 1, map_width);

                    if (!hash::has(terrain_tiles, above) || hash::get(terrain_tiles, above, Tile::None) != Tile::Floor) {
                        hash::set(placeholder_walls, above, true);
                    }

                    if (!hash::has(terrain_tiles, below) || hash::get(terrain_tiles, below, Tile::None) != Tile::Floor) {
                        hash::set(placeholder_walls, below, true);
                    }
                } else if (prev.x == next.x && prev.y != next.y) { // Vertical line
                    assert(coord.x > 0);

                    int32_t left = index(coord.x - 1, coord.y, map_width);
                    int32_t right = index(coord.x + 1, coord.y, map_width);

                    if (!hash::has(terrain_tiles, left) || hash::get(terrain_tiles, left, Tile::None) != Tile::Floor) {
                        hash::set(placeholder_walls, left, true);
                    }

                    if (!hash::has(terrain_tiles, right) || hash::get(terrain_tiles, right, Tile::None) != Tile::Floor) {
                        hash::set(placeholder_walls, right, true);
                    }
                } else { // Corner
                    for (int y = -1; y <= 1; ++y) {
                        for (int x = -1; x <= 1; ++x) {
                            if (x == 0 && y == 0) {
                                continue;
                            }

                            if ((coord.x - x == next.x && coord.y - y == next.y) ||
                                (coord.x - x == prev.x && coord.y - y == prev.y)) {
                                continue;
                            }

                            int32_t adjacent_index = index(coord.x - x, coord.y - y, map_width);

                            if (!hash::has(terrain_tiles, adjacent_index) || hash::get(terrain_tiles, adjacent_index, Tile::None) != Tile::Floor) {
                                hash::set(placeholder_walls, adjacent_index, true);
                            }
                        }
                    }
                }
            }
        });

        // Function to update the correct wall tile on a coordinate. Depends on surrounding walls and placeholder walls.
        auto place_wall = [&](int32_t index_wall) {
            int32_t coord_x, coord_y;
            coord(index_wall, coord_x, coord_y, map_width);

            const int32_t index_nw = index(coord_x - 1, coord_y - 1, map_width);
            const int32_t index_n = index(coord_x, coord_y - 1, map_width);
            const int32_t index_ne = index(coord_x + 1, coord_y - 1, map_width);
            const int32_t index_w = index(coord_x - 1, coord_y, map_width);
            const int32_t index_e = index(coord_x + 1, coord_y, map_width);
            const int32_t index_sw = index(coord_x - 1, coord_y + 1, map_width);
            const int32_t index_s = index(coord_x, coord_y + 1, map_width);
            const int32_t index_se = index(coord_x + 1, coord_y + 1, map_width);

#pragma warning(push)
#pragma warning(disable : 4189)

            const bool wall_nw = (hash::has(terrain_tiles, index_nw) && hash::get(terrain_tiles, index_nw, Tile::None) != Tile::Floor) || hash::has(placeholder_walls, index_nw);
            const bool wall_n = (hash::has(terrain_tiles, index_n) && hash::get(terrain_tiles, index_n, Tile::None) != Tile::Floor) || hash::has(placeholder_walls, index_n);
            const bool wall_ne = (hash::has(terrain_tiles, index_ne) && hash::get(terrain_tiles, index_ne, Tile::None) != Tile::Floor) || hash::has(placeholder_walls, index_ne);
            const bool wall_w = (hash::has(terrain_tiles, index_w) && hash::get(terrain_tiles, index_w, Tile::None) != Tile::Floor) || hash::has(placeholder_walls, index_w);
            const bool wall_e = (hash::has(terrain_tiles, index_e) && hash::get(terrain_tiles, index_e, Tile::None) != Tile::Floor) || hash::has(placeholder_walls, index_e);
            const bool wall_sw = (hash::has(terrain_tiles, index_sw) && hash::get(terrain_tiles, index_sw, Tile::None) != Tile::Floor) || hash::has(placeholder_walls, index_sw);
            const bool wall_s = (hash::has(terrain_tiles, index_s) && hash::get(terrain_tiles, index_s, Tile::None) != Tile::Floor) || hash::has(placeholder_walls, index_s);
            const bool wall_se = (hash::has(terrain_tiles, index_sw) && hash::get(terrain_tiles, index_se, Tile::None) != Tile::Floor) || hash::has(placeholder_walls, index_se);

            const bool floor_nw = hash::has(terrain_tiles, index_nw) && hash::get(terrain_tiles, index_nw, Tile::None) == Tile::Floor;
            const bool floor_n = hash::has(terrain_tiles, index_n) && hash::get(terrain_tiles, index_n, Tile::None) == Tile::Floor;
            const bool floor_ne = hash::has(terrain_tiles, index_ne) && hash::get(terrain_tiles, index_ne, Tile::None) == Tile::Floor;
            const bool floor_w = hash::has(terrain_tiles, index_w) && hash::get(terrain_tiles, index_w, Tile::None) == Tile::Floor;
            const bool floor_e = hash::has(terrain_tiles, index_e) && hash::get(terrain_tiles, index_e, Tile::None) == Tile::Floor;
            const bool floor_sw = hash::has(terrain_tiles, index_sw) && hash::get(terrain_tiles, index_sw, Tile::None) == Tile::Floor;
            const bool floor_s = hash::has(terrain_tiles, index_s) && hash::get(terrain_tiles, index_s, Tile::None) == Tile::Floor;
            const bool floor_se = hash::has(terrain_tiles, index_se) && hash::get(terrain_tiles, index_se, Tile::None) == Tile::Floor;

            // These signify that the walls around the coordinate are room walls, not placeholder corridor walls.
            const bool wall_w_room = !hash::has(placeholder_walls, index_w) && wall_w;
            const bool wall_e_room = !hash::has(placeholder_walls, index_e) && wall_e;
            const bool wall_n_room = !hash::has(placeholder_walls, index_n) && wall_n;
            const bool wall_s_room = !hash::has(placeholder_walls, index_s) && wall_s;

#pragma warning(pop)

            if (floor_e && floor_w && wall_n && wall_s) { // Vertical wall between two floor tiles
                hash::set(terrain_tiles, index_wall, Tile::WallLeftRight);
            } else if (floor_e && floor_w && floor_n && wall_s) { // Vertical wall cap north between two floor tiles
                hash::set(terrain_tiles, index_wall, Tile::WallCapTop);
            } else if (floor_e && floor_w && floor_s && wall_n) { // Vertical wall cap south between two floor tiles
                hash::set(terrain_tiles, index_wall, Tile::WallCapBottom);
            } else if (floor_n && floor_s && wall_w && wall_e) { // Horizontal wall between two floor tiles
                hash::set(terrain_tiles, index_wall, Tile::WallTopBottom);
            } else if (floor_n && floor_s && wall_w && floor_e) { // Horizontal wall cap east between two floor tiles
                hash::set(terrain_tiles, index_wall, Tile::WallCapRight);
            } else if (floor_n && floor_s && floor_w && wall_e) { // Horizontal wall cap west between two floor tiles
                hash::set(terrain_tiles, index_wall, Tile::WallCapLeft);
            } else if (wall_n && wall_s && floor_e) { // Vertical wall left of corridor
                hash::set(terrain_tiles, index_wall, Tile::WallLeft);
            } else if (wall_n && wall_s && floor_w) { // Vertical wall right of corridor
                hash::set(terrain_tiles, index_wall, Tile::WallRight);
            } else if (wall_w && wall_e && floor_s) { // Horizontal wall, above corridor
                hash::set(terrain_tiles, index_wall, Tile::WallTop);
            } else if (wall_w && wall_e && floor_n) { // Horizontal wall, below corridor
                hash::set(terrain_tiles, index_wall, Tile::WallBottom);
            } else if (wall_n && wall_e && floor_w && floor_s && floor_sw) { // Corner right and up above a corridor └
                hash::set(terrain_tiles, index_wall, Tile::CorridorCornerUpRight);
            } else if (wall_s && wall_e && floor_n && floor_w && floor_nw) { // Corner right and down below a corridor ┌
                hash::set(terrain_tiles, index_wall, Tile::CorridorCornerDownRight);
            } else if (wall_n && wall_w && floor_s && floor_e && floor_se) { // Corner left and up above a corridor ┘
                hash::set(terrain_tiles, index_wall, Tile::CorridorCornerUpLeft);
            } else if (wall_s && wall_w && floor_n && floor_e && floor_ne) { // Corner left and down below a corridor ┐
                hash::set(terrain_tiles, index_wall, Tile::CorridorCornerDownLeft);
            } else if (wall_s && wall_e && floor_se) { // Corner cap left and down above a corridor
                hash::set(terrain_tiles, index_wall, Tile::WallCornerTopLeft);
            } else if (wall_s && wall_w && floor_sw) { // Corner cap right and down above a corridor
                hash::set(terrain_tiles, index_wall, Tile::WallCornerTopRight);
            } else if (wall_n && wall_e && floor_ne) { // Corner cap right and up below a corridor
                hash::set(terrain_tiles, index_wall, Tile::WallCornerBottomLeft);
            } else if (wall_n && wall_w && floor_nw) { // Corner cap left and up below a corridor
                hash::set(terrain_tiles, index_wall, Tile::WallCornerBottomRight);
            } else {
                // TODO: Handle tri-wall corners
                // hash::set(terrain_tiles, index_wall, Tile::Missing);
                hash::set(terrain_tiles, index_wall, Tile::Wall);
            }
        };

        // Update placeholder walls
        for (auto iter = hash::begin(placeholder_walls); iter != hash::end(placeholder_walls); ++iter) {
            int32_t index = (int32_t)iter->key;
            place_wall(index);
        }
    }

    // Add mobs
    {
        std::scoped_lock lock(*game->dungen_mutex);

        for (auto it = hash::begin(game->enemy_mobs); it != hash::end(game->enemy_mobs); ++it) {
            MAKE_DELETE(game->allocator, Mob, it->value);
        }

        uint64_t mob_id = 0;
        int non_boss_monster_count = 0;
        int boss_monster_count = 0;

        for (MobTemplate **it = array::begin(game->mob_templates->mob_templates); it != array::end(game->mob_templates->mob_templates); ++it) {
            if (((*it)->tags & MobTemplate::Tags::MobTemplateTagsBoss) == MobTemplate::Tags::MobTemplateTagsBoss) {
                ++boss_monster_count;
            } else {
                ++non_boss_monster_count;
            }
        }

        /**
         * @brief A function to return a pointer to random MobTemplate.
         * 
         */
        auto random_mob = [&](bool is_boss) -> MobTemplate * {
            int mob_template_index;

            if (is_boss) {
                mob_template_index = rnd_pcg_range(&random_device, 0, boss_monster_count - 1);
            } else {
                mob_template_index = rnd_pcg_range(&random_device, 0, non_boss_monster_count - 1);
            }

            int mob_template_count = 0;

            for (MobTemplate **mob_template_it = array::begin(game->mob_templates->mob_templates); mob_template_it != array::end(game->mob_templates->mob_templates); ++mob_template_it) {
                if (((*mob_template_it)->tags & MobTemplate::Tags::MobTemplateTagsBoss) == MobTemplate::Tags::MobTemplateTagsBoss) {
                    if (!is_boss) {
                        continue;
                    }
                } else {
                    if (is_boss) {
                        continue;
                    }
                }

                if (mob_template_count == mob_template_index) {
                    return *mob_template_it;
                    break;
                }

                ++mob_template_count;
            }

            return nullptr;
        };

        for (auto it = hash::begin(rooms); it != hash::end(rooms); ++it) {
            const Room &room = it->value;
            if (room.start_room) {
                continue;
            }

            const RoomTemplate &room_template = *game->room_templates->room_templates[room.room_template_index];

            Array<uint32_t> floor_indices(ta);

            for (uint32_t i = 0; i < array::size(*room_template.tiles); ++i) {
                RoomTemplate::TileType tile_type = static_cast<RoomTemplate::TileType>((*room_template.tiles)[i]);

                if (tile_type == RoomTemplate::TileType::Floor) {
                    array::push_back(floor_indices, i);
                }
            }

            // Boss spawn
            if (room.boss_room) {
                for (uint32_t i = 0; i < array::size(*room_template.tiles); ++i) {
                    RoomTemplate::TileType tile_type = static_cast<RoomTemplate::TileType>((*room_template.tiles)[i]);

                    if (tile_type == RoomTemplate::TileType::Stair) {
                        int32_t x, y;
                        coord(i, x, y, room_template.columns);
                        y = room_template.rows - y - 1;

                        Mob *mob = MAKE_NEW(game->allocator, Mob);
                        ++mob_id;

                        hash::set(game->enemy_mobs, mob_id, mob);

                        mob->tile_index = index(room.x + x, room.y + y, map_width);
                        mob->mob_template = random_mob(true);

                        if (!mob->mob_template) {
                            log_fatal("Missing mob template");
                        }

                        break;
                    }
                }
            }

            // Regular mob spawns
            {
                const int32_t max_spawns = array::size(floor_indices) < max_room_spawns ? array::size(floor_indices) : max_room_spawns;
                const int32_t num_spawns = rnd_pcg_range(&random_device, 1, max_spawns);

                int count_spawns = 0;
                Hash<bool> spawned_indices(ta);
                while (count_spawns < num_spawns) {
                    int floor_index = rnd_pcg_range(&random_device, 0, array::size(floor_indices) - 1);
                    if (hash::has(spawned_indices, floor_index)) {
                        continue;
                    } else {
                        hash::set(spawned_indices, floor_index, true);
                    }

                    int32_t x, y;
                    coord(floor_indices[floor_index], x, y, room_template.columns);
                    y = room_template.rows - y - 1;

                    Mob *mob = MAKE_NEW(game->allocator, Mob);
                    ++mob_id;

                    hash::set(game->enemy_mobs, mob_id, mob);

                    mob->tile_index = index(room.x + x, room.y + y, map_width);
                    mob->mob_template = random_mob(false);

                    if (!mob->mob_template) {
                        log_fatal("Missing mob template");
                    }

                    ++count_spawns;

                    if (count_spawns > 255) {
                        log_error("Could not place %u spaws in room %s", num_spawns, string_stream::c_str(*room_template.name));
                        break;
                    }
                }
            }
        }
    }

    // Finalize and output.
    {
        std::scoped_lock lock(*game->dungen_mutex);

        MAKE_DELETE(game->allocator, Level, game->level);
        game->level = MAKE_NEW(game->allocator, Level, game->allocator);

        game->level->tiles = terrain_tiles;
        game->level->rooms = rooms;
        game->level->max_width = map_width;
        game->level->stairs_up_index = stairs_up_index;
        game->level->stairs_down_index = stairs_down_index;

        game->dungen_done = true;
    }
}

} // namespace game
