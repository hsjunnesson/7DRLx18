#include "dungen.h"
#include "game.h"
#include "line.hpp"

#pragma warning(push, 0)
#include "engine/config.inl"
#include "engine/engine.h"
#include "engine/log.h"
#include "engine/sprites.h"
#include "engine/math.inl"
#include "proto/game.pb.h"

#include <array.h>
#include <hash.h>
#include <memory.h>
#include <murmur_hash.h>
#include <queue.h>
#include <temp_allocator.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cassert>
#include <mutex>
#include <random>
#include <thread>
#pragma warning(pop)

namespace game {
using namespace foundation;
using engine::coord;
using engine::index;

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

    TempAllocator1024 allocator;

    DungenParams params;
    engine::config::read(game->params->dungen_params_filename().c_str(), &params);

    Hash<Tile> terrain_tiles = Hash<Tile>(allocator);

    const uint32_t map_width = params.map_width();
    const uint32_t rooms_count_wide = (uint32_t)ceil(sqrt(params.room_count()));
    const uint32_t rooms_count_tall = (uint32_t)ceil(sqrt(params.room_count()));
    const uint32_t section_width = map_width / rooms_count_wide;
    const uint32_t section_height = map_width / rooms_count_tall;

    log_debug("Dungen rooms count wide %u", rooms_count_wide);
    log_debug("Dungen rooms count tall %u", rooms_count_tall);
    log_debug("Dungen section width %u", section_width);
    log_debug("Dungen section height %u", section_height);

    std::random_device random_device;
    std::mt19937 random_engine(random_device());
    unsigned int seed = (unsigned int)time(nullptr);
    // seed = 1642864096; // orphaned island
//    seed = 1644687731;

    random_engine.seed(seed);

    log_debug("Dungen seeded with %u", seed);

    std::uniform_int_distribution<int32_t> room_size_distribution(params.min_room_size(), params.max_room_size());
    std::uniform_int_distribution<int> fifty_fifty(0, 1);
    std::uniform_int_distribution<int> percentage(1, 100);

    uint32_t start_room_index = 0;
    uint32_t boss_room_index = 0;

    uint32_t stairs_up_index = 0;
    uint32_t stairs_down_index = 0;

    // Rooms and corridors collections
    Hash<Room> rooms = Hash<Room>(allocator);
    Array<Corridor> corridors = Array<Corridor>(allocator);

    // Decide whether main orientation is vertical or horizontal
    {
        bool start_room_vertical_side = fifty_fifty(random_engine) == 0;
        bool start_room_first_side = fifty_fifty(random_engine) == 0;

        if (start_room_vertical_side) {
            std::uniform_int_distribution<int32_t> tall_side_distribution(0, rooms_count_tall - 1);
            int32_t y = tall_side_distribution(random_engine);
            start_room_index = y * rooms_count_wide;

            if (!start_room_first_side) {
                start_room_index += (rooms_count_wide - 1);
            }

            y = tall_side_distribution(random_engine);
            boss_room_index = y * rooms_count_wide;

            if (start_room_first_side) {
                boss_room_index += (rooms_count_wide - 1);
            }
        } else {
            std::uniform_int_distribution<int32_t> wide_side_distribution(0, rooms_count_wide - 1);
            int32_t x = wide_side_distribution(random_engine);
            start_room_index = x;

            if (!start_room_first_side) {
                start_room_index += rooms_count_wide * (rooms_count_tall - 1);
            }

            x = wide_side_distribution(random_engine);
            boss_room_index = x;

            if (start_room_first_side) {
                start_room_index += rooms_count_wide * (rooms_count_tall - 1);
            }
        }

        if (start_room_index > (uint32_t)params.room_count()) {
            start_room_index = params.room_count() - 1;
        }

        if (boss_room_index > (uint32_t)params.room_count()) {
            boss_room_index = params.room_count() - 1;
        }

        log_debug("Dungen start room index %d", start_room_index);
        log_debug("Dungen boss room index %d", boss_room_index);
    }

    // Place rooms in grids sections, referenced by their index.
    {
        for (uint32_t room_index = 0; room_index < (uint32_t)params.room_count(); ++room_index) {
            uint32_t room_index_x, room_index_y;
            coord(room_index, room_index_x, room_index_y, rooms_count_wide);

            const uint32_t section_min_x = room_index_x * section_width;
            const uint32_t section_max_x = section_min_x + section_width;
            const uint32_t section_min_y = room_index_y * section_height;
            const uint32_t section_max_y = section_min_y + section_height;

            const uint32_t room_width = room_size_distribution(random_engine);
            const uint32_t room_height = room_size_distribution(random_engine);

            std::uniform_int_distribution<uint32_t> x_offset(section_min_x + 2, section_max_x - 2 - room_width);
            std::uniform_int_distribution<uint32_t> y_offset(section_min_y + 2, section_max_y - 2 - room_height);

            const uint32_t room_x = x_offset(random_engine);
            const uint32_t room_y = y_offset(random_engine);

            Room room = Room{room_index, room_x, room_y, room_width, room_height};

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
        uint32_t start_room_x, start_room_y;
        coord(start_room_index, start_room_x, start_room_y, rooms_count_wide);

        uint32_t boss_room_x, boss_room_y;
        coord(boss_room_index, boss_room_x, boss_room_y, rooms_count_wide);

        Array<line::Coordinate> shortest_line_path = line::zig_zag(allocator, {(int32_t)start_room_x, (int32_t)start_room_y}, {(int32_t)boss_room_x, (int32_t)boss_room_y});

        for (int32_t i = 0; i < (int32_t)array::size(shortest_line_path) - 1; ++i) {
            line::Coordinate from = shortest_line_path[i];
            line::Coordinate to = shortest_line_path[i + 1];

            uint32_t from_room_index = index((uint32_t)from.x, (uint32_t)from.y, rooms_count_wide);
            uint32_t to_room_index = index((uint32_t)to.x, (uint32_t)to.y, rooms_count_wide);

            array::push_back(corridors, Corridor{from_room_index, to_room_index});
        }
    }

    // Expand some branches
    {
        Array<Corridor> branches = Array<Corridor>(allocator);

        // Returns a corridor to a random adjacent room, which isn't already connected to this room.
        auto expand = [&](uint32_t room_index) {
            uint32_t room_x, room_y;
            coord(room_index, room_x, room_y, rooms_count_wide);

            Array<uint32_t> available_adjacent_room_indices = Array<uint32_t>(allocator);

            auto adjacent_coordinates = {
                line::Coordinate{(int32_t)room_x + 1, (int32_t)room_y},
                line::Coordinate{(int32_t)room_x,     (int32_t)room_y + 1},
                line::Coordinate{(int32_t)room_x - 1, (int32_t)room_y},
                line::Coordinate{(int32_t)room_x,     (int32_t)room_y - 1}};

            // For each orthogonally adjacent room. Check if it's a valid location.
            for (line::Coordinate next_coordinate : adjacent_coordinates) {
                if (next_coordinate.x >= 0 &&
                    next_coordinate.x < (int32_t)rooms_count_wide &&
                    next_coordinate.y >= 0 &&
                    next_coordinate.y < (int32_t)rooms_count_tall) {
                    uint32_t next_room_index = index((uint32_t)next_coordinate.x, (uint32_t)next_coordinate.y, rooms_count_wide);
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
                std::uniform_int_distribution<int> random_distribution(0, array::size(available_adjacent_room_indices) - 1);
                int random_selection = random_distribution(random_engine);
                uint32_t next_room_index = available_adjacent_room_indices[random_selection];
                Corridor branching_corridor = Corridor{room_index, next_room_index};
                array::push_back(corridors, branching_corridor);
                return next_room_index;
            }

            return (uint32_t)0;
        };

        for (auto iter = array::begin(corridors); iter != array::end(corridors); ++iter) {
            // Don't branch from start or end room
            if (iter == array::begin(corridors)) {
                continue;
            }

            Corridor corridor = *iter;
            uint32_t room_index = corridor.from_room_index;

            // Expand branches, perhaps multiple times
            bool expansion_done = percentage(random_engine) > params.expand_chance();
            while (!expansion_done) {
                uint32_t next_room_index = expand(room_index);
                if (next_room_index >= 0) {
                    room_index = next_room_index;
                    expansion_done = percentage(random_engine) > params.expand_chance();
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
    {
        Queue<uint32_t> disconnected_room_indices = Queue<uint32_t>(allocator);
        for (uint32_t i = 0; i < (uint32_t)params.room_count(); ++i) {
            bool found = false;

            for (auto iter = array::begin(corridors); iter != array::end(corridors); ++iter) {
                Corridor &corridor = *iter;
                if (corridor.from_room_index == i || corridor.to_room_index == i) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                queue::push_front(disconnected_room_indices, i);
            }
        }

        for (uint32_t i = 0; i < (uint32_t)queue::size(disconnected_room_indices); ++i) {
            uint32_t index = disconnected_room_indices[i];
            hash::remove(rooms, index);
        }
    }

    // Move map to origin
    {
        uint32_t min_x = INT32_MAX;
        uint32_t min_y = INT32_MAX;

        for (auto iter = hash::begin(rooms); iter != hash::end(rooms); ++iter) {
            Room &room = iter->value;
            if (room.x < min_x) {
                min_x = room.x;
            }

            if (room.y < min_y) {
                min_y = room.y;
            }
        }

        for (auto iter = hash::begin(rooms); iter != hash::end(rooms); ++iter) {
            Room &room = iter->value;
            room.x -= min_x;
            room.y -= min_y;
        }
    }

    // Draw rooms as tiles
    {
        Tile floor_tile = Tile::Floor;

        for (auto iter = hash::begin(rooms); iter != hash::end(rooms); ++iter) {
            Room &room = iter->value;

            for (uint32_t y = 0; y < room.h; ++y) {
                for (uint32_t x = 0; x < room.w; ++x) {
                    Tile tile = Tile::None;

                    if (y == 0) {
                        if (x == 0) {
                            tile = Tile::WallCornerTopLeft;
                        } else if (x == room.w - 1) {
                            tile = Tile::WallCornerTopRight;
                        } else {
                            tile = Tile::WallTop;
                        }
                    } else if (y == room.h - 1) {
                        if (x == 0) {
                            tile = Tile::WallCornerBottomLeft;
                        } else if (x == room.w - 1) {
                            tile = Tile::WallCornerBottomRight;
                        } else {
                            tile = Tile::WallBottom;
                        }
                    } else if (x == 0) {
                        tile = Tile::WallLeft;
                    } else if (x == room.w - 1) {
                        tile = Tile::WallRight;
                    } else {
                        tile = floor_tile;
                    }

                    hash::set(terrain_tiles, index(room.x + x, room.y + y, map_width), {tile});
                }
            }
        }
    }

    // Add stairs
    {
        const Tile stairs_down_tile = Tile::StairsDown;
        const Tile stairs_up_tile = Tile::StairsUp;

        Room start_room = hash::get(rooms, start_room_index, {});
        if (start_room.start_room) {
            // TODO: randomize this
            stairs_up_index = index(start_room.x + start_room.w / 2, start_room.y + start_room.h / 2, map_width);
            hash::set(terrain_tiles, stairs_up_index, {stairs_up_tile});
        }

        Room boss_room = hash::get(rooms, boss_room_index, {});
        if (boss_room.boss_room) {
            // TODO: randomize this
            stairs_down_index = index(boss_room.x + boss_room.w / 2, boss_room.y + boss_room.h / 2, map_width);
            hash::set(terrain_tiles, stairs_down_index, {stairs_down_tile});
        }
    }

    // Draw corridors as terrain_tiles
    {
        // Added walls which needs to be properly placed.
        Hash<bool> placeholder_walls = Hash<bool>(allocator);

        // Function to iterate through corridors, applying a function on each coordinate.
        auto iterate_corridor = [&](std::function<void(line::Coordinate prev, line::Coordinate coord, line::Coordinate next)> apply) {
            for (auto iter = array::begin(corridors); iter != array::end(corridors); ++iter) {
                Corridor corridor = *iter;

                Room start_room = hash::get(rooms, corridor.from_room_index, {});
                Room to_room = hash::get(rooms, corridor.to_room_index, {});

                line::Coordinate a = {(int32_t)start_room.x + (int32_t)start_room.w / 2, (int32_t)start_room.y + (int32_t)start_room.h / 2};
                line::Coordinate b = {(int32_t)to_room.x + (int32_t)to_room.w / 2, (int32_t)to_room.y + (int32_t)to_room.h / 2};

                Array<line::Coordinate> coordinates = line::zig_zag(allocator, a, b);

                for (int32_t line_i = 0; line_i < (int32_t)array::size(coordinates); ++line_i) {
                    line::Coordinate prev;
                    if (line_i > 0) {
                        prev = coordinates[line_i - 1];
                    } else {
                        prev.x = -1;
                        prev.y = -1;
                    }

                    line::Coordinate coord = coordinates[line_i];

                    // Don't place corridors if we're inside the start or to room.
                    // But place corridor in the wall of the rooms.
                    bool inside_start_room =
                        (uint32_t)coord.x > start_room.x &&
                        (uint32_t)coord.y > start_room.y &&
                        (uint32_t)coord.x < start_room.x + start_room.w - 1 &&
                        (uint32_t)coord.y < start_room.y + start_room.h - 1;

                    bool inside_to_room =
                        (uint32_t)coord.x > to_room.x &&
                        (uint32_t)coord.y > to_room.y &&
                        (uint32_t)coord.x < to_room.x + to_room.w - 1 &&
                        (uint32_t)coord.y < to_room.y + to_room.h - 1;

                    if (inside_start_room || inside_to_room) {
                        continue;
                    }

                    line::Coordinate next;
                    if (line_i < (int32_t)array::size(coordinates) - 1) {
                        next = coordinates[line_i + 1];
                    } else {
                        next.x = -1;
                        next.y = -1;
                    }

                    apply(prev, coord, next);
                }
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
                    uint32_t above = index(coord.x, coord.y - 1, map_width);
                    uint32_t below = index(coord.x, coord.y + 1, map_width);

                    if (!hash::has(terrain_tiles, above) || hash::get(terrain_tiles, above, Tile::None) != Tile::Floor) {
                        hash::set(placeholder_walls, above, true);
                    }

                    if (!hash::has(terrain_tiles, below) || hash::get(terrain_tiles, below, Tile::None) != Tile::Floor) {
                        hash::set(placeholder_walls, below, true);
                    }
                } else if (prev.x == next.x && prev.y != next.y) { // Vertical line
                    uint32_t left = index(coord.x - 1, coord.y, map_width);
                    uint32_t right = index(coord.x + 1, coord.y, map_width);

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

                            uint32_t adjacent_index = index(coord.x - x, coord.y - y, map_width);

                            if (!hash::has(terrain_tiles, adjacent_index) || hash::get(terrain_tiles, adjacent_index, Tile::None) != Tile::Floor) {
                                hash::set(placeholder_walls, adjacent_index, true);
                            }
                        }
                    }
                }
            }
        });

        // Function to update the correct wall tile on a coordinate. Depends on surrounding walls and placeholder walls.
        auto place_wall = [&](uint32_t index_wall) {
            uint32_t coord_x, coord_y;
            coord(index_wall, coord_x, coord_y, map_width);

            const uint32_t index_nw = index(coord_x - 1, coord_y - 1, map_width);
            const uint32_t index_n = index(coord_x, coord_y - 1, map_width);
            const uint32_t index_ne = index(coord_x + 1, coord_y - 1, map_width);
            const uint32_t index_w = index(coord_x - 1, coord_y, map_width);
            const uint32_t index_e = index(coord_x + 1, coord_y, map_width);
            const uint32_t index_sw = index(coord_x - 1, coord_y + 1, map_width);
            const uint32_t index_s = index(coord_x, coord_y + 1, map_width);
            const uint32_t index_se = index(coord_x + 1, coord_y + 1, map_width);

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

            // TODO: Handle tri-wall corners

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
                hash::set(terrain_tiles, index_wall, Tile::Missing);
            }
        };

        // Update placeholder walls
        for (auto iter = hash::begin(placeholder_walls); iter != hash::end(placeholder_walls); ++iter) {
            uint32_t index = (uint32_t)iter->key;
            place_wall(index);
        }
    }

    // Finalize and output.
    {
        std::scoped_lock lock(*game->dungen_mutex);
        hash::clear(game->level->tiles);
        hash::clear(game->level->rooms);

        game->level->tiles = terrain_tiles;
        game->level->rooms = rooms;
        game->level->max_width = map_width;
        game->level->stairs_up_index = stairs_up_index;
        game->level->stairs_down_index = stairs_down_index;
    }

    game::transition(*engine, game, GameState::Playing);
}

} // namespace game
