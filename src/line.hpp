#pragma once

#include <functional>
#include <stdint.h>

#pragma warning(push, 0)
#include "array.h"
#include "memory.h"
#include "temp_allocator.h"
#pragma warning(pop)

// Line drawing algorithms.
namespace line {
using namespace foundation;

// An integer coordinate.
struct Coordinate {
    int32_t x, y;
};

/**
 * @brief The mode of the line, whether lines are allowed to move diagonal or only orthogonal.
 * 
 * These are diagonal lines
 * 
 *       ###
 *    ###
 * ###
 * 
 * These are only orthogonal
 * 
 *     ###
 *   ###
 * ###
 * 
 */
enum class LineMode {
    // Allow diagonal lines.
    AllowDiagonal,

    // Only allow orthogonal movement between coordinates.
    OnlyOrthogonal,
};

// Line of sight blocking callback function pointer
typedef std::function<bool(int32_t x, int32_t y)> is_clear;

/**
 * @brief Line of sight checking between two coordinates.
 * 
 * @param a The first coordinate.
 * @param b The second coordinate.
 * @param clear The callback function used to query the state of the world. Returns true if a coordinate is clear of line of sight blockers.
 * @param line_mode Which line mode to use.
 * @return true If there exists line of sight between the two coordinates.
 */
bool los(const Coordinate a, const Coordinate b, is_clear clear, LineMode line_mode = LineMode::AllowDiagonal) {
    int32_t sx, sy, dx, dy;
    int32_t x0 = a.x;
    int32_t y0 = a.y;
    int32_t x1 = b.x;
    int32_t y1 = b.y;

    if (x0 < x1) {
        sx = 1;
        dx = x1 - x0;
    } else {
        sx = -1;
        dx = x0 - x1;
    }

    if (y0 < y1) {
        sy = 1;
        dy = y1 - y0;
    } else {
        sy = -1;
        dy = y0 - y1;
    }

    int32_t err = dx - dy;
    int32_t e2 = 0;

    if (!clear(x0, y0)) {
        return false;
    }

    while (!(x0 == x1 && y0 == y1)) {
        e2 = err + err;

        if (line_mode == LineMode::AllowDiagonal) {
            if (e2 > -dy) {
                err = err - dy;
                x0 = x0 + sx;
            }

            if (e2 < dx) {
                err = err + dx;
                y0 = y0 + sy;
            }
        } else {
            if (e2 > -dy) {
                err = err - dy;
                x0 = x0 + sx;
            } else {
                err = err + dx;
                y0 = y0 + sy;
            }
        }

        if (!clear(x0, y0)) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Returns an array of coordinates describing the straight line betwen two coordinates. 
 * This method allows diagonal movement between coordinates.
 * Optionally checking with a callback whether the line is blocked. The resulting coordinates will not contain the blocking coordinate.
 * 
 * @param allocator The Allocator to use for the Array.
 * @param a The first coordinate.
 * @param b The second coordinate.
 * @param clear The callback function used to query the state of the world. Returns true if a coordinate is clear of line of sight blockers.
 * @param blocked When the optional is_clear function is supplied, this is set to whether the line was blocked.
 * @param line_mode Which line mode to use.
 * @return Array<Coordinate> The array of coordinates.
 */
Array<Coordinate> line(Allocator &allocator, Coordinate a, Coordinate b, is_clear clear = nullptr, bool *blocked = nullptr, LineMode line_mode = LineMode::AllowDiagonal) {
    Array<Coordinate> coordinates(allocator);
    bool clear_line = los(
        a, b, [clear, &coordinates](int32_t x, int32_t y) {
            if (clear && !clear(x, y)) {
                return false;
            }

            array::push_back(coordinates, Coordinate{x, y});
            return true;
        },
        line_mode);

    if (blocked) {
        *blocked = !clear_line;
    }

    return coordinates;
}

/**
 * @brief Creates a zig zag line between two coordinates.
 * The line will change direction halfway between, and move in like a Manhattan distance way.
 * 
 *     #####
 *     #
 *     #
 * #####
 * 
 * @param allocator The Allocator to use for the Array.
 * @param a The first coordinate.
 * @param b The second coordinate.
 * @return Array<Coordinate> The array of coordinates.
 */
Array<Coordinate> zig_zag(Allocator &allocator, Coordinate a, Coordinate b) {
    Array<Coordinate> coordinates(allocator);

    int32_t x0 = a.x;
    int32_t y0 = a.y;
    int32_t x1 = b.x;
    int32_t y1 = b.y;

    int32_t xoffset = 0;
    int32_t yoffset = 0;

    if (x0 < x1) {
        xoffset = 1;
    } else if (x0 > x1) {
        xoffset = -1;
    }

    if (y0 < y1) {
        yoffset = 1;
    } else if (y0 > y1) {
        yoffset = -1;
    }

    if (xoffset == 0 && yoffset == 0) {
        return coordinates;
    }

    int32_t half_x = x0 + (int32_t)round((x1 - x0) / 2);
    int32_t half_y = y0 + (int32_t)round((y1 - y0) / 2);

    int32_t x = x0;
    int32_t y = y0;

    bool horizontal = abs(x1 - x0) > abs(y1 - y0);

    array::push_back(coordinates, Coordinate{x, y});

    while (!(x == x1 && y == y1)) {
        int32_t sx, sy;

        if (horizontal) {
            sx = xoffset;
            sy = 0;

            if (x == half_x) {
                if (y == y1) {
                    sx = xoffset;
                    sy = 0;
                } else {
                    sx = 0;
                    sy = yoffset;
                }
            }
        } else {
            sx = 0;
            sy = yoffset;

            if (y == half_y) {
                if (x == x1) {
                    sx = 0;
                    sy = yoffset;
                } else {
                    sx = xoffset;
                    sy = 0;
                }
            }
        }

        x = x + sx;
        y = y + sy;

        array::push_back(coordinates, Coordinate{x, y});
    }

    return coordinates;
}

} // namespace line
