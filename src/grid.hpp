#pragma once

#include <stdint.h>

namespace grid {

/**
 * @brief Returns the index of an x, y coordinate
 * 
 * @param x The x coord
 * @param y The y coord
 * @param max_width The maximum width.
 * @return The index.
 */
constexpr int32_t index(int32_t const x, int32_t const y, int32_t const max_width) {
    return x + max_width * y;
}

/**
 * @brief Calculates the x, y coordinates based on an index.
 * 
 * @param index The index.
 * @param x The pass-by-reference x coord to calculate.
 * @param y The pass-by-reference y coord to calculate.
 * @param max_width The maximum width.
 */
constexpr void coord(int32_t const index, int32_t &x, int32_t &y, int32_t const max_width) {
    x = index % max_width;
    y = index / max_width;
}

/**
 * @brief Returns a new index offset by x and y coordinates.
 * 
 * @param idx The index.
 * @param xoffset The x offset.
 * @param yoffset They y offset.
 * @param max_width The maximum width.
 * @return The index.
 */
constexpr int32_t index_offset(int32_t const idx, int32_t const xoffset, int32_t const yoffset, int32_t const max_width) {
    int32_t x = 0;
    int32_t y = 0;
    coord(idx, x, y, max_width);

    return index(x + xoffset, y + yoffset, max_width);
}


} // namespace grid
