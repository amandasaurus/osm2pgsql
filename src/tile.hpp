#ifndef OSM2PGSQL_TILE_HPP
#define OSM2PGSQL_TILE_HPP

/**
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This file is part of osm2pgsql (https://osm2pgsql.org/).
 *
 * Copyright (C) 2006-2025 by the osm2pgsql developer community.
 * For a full list of authors see the git log.
 */

#include "geom-box.hpp"
#include "geom.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

class quadkey_t
{
public:
    quadkey_t() noexcept = default;

    explicit quadkey_t(uint64_t value) noexcept : m_value(value) {}

    uint64_t value() const noexcept { return m_value; }

    /**
     * Calculate quad key with the given number of zoom levels down from the
     * zoom level of this quad key.
     */
    quadkey_t down(uint32_t levels) const noexcept
    {
        quadkey_t qk;
        qk.m_value = m_value >> (levels * 2);
        return qk;
    }

    friend bool operator==(quadkey_t a, quadkey_t b) noexcept
    {
        return a.m_value == b.m_value;
    }

    friend bool operator!=(quadkey_t a, quadkey_t b) noexcept
    {
        return !(a == b);
    }

    friend bool operator<(quadkey_t a, quadkey_t b) noexcept
    {
        return a.value() < b.value();
    }

private:
    uint64_t m_value = std::numeric_limits<uint64_t>::max();
}; // class quadkey_t

template <>
struct std::hash<quadkey_t>
{
    std::size_t operator()(quadkey_t quadkey) const noexcept
    {
        return quadkey.value();
    }
};

using quadkey_list_t = std::vector<quadkey_t>;

/**
 * A tile in the usual web tile format.
 */
class tile_t
{
public:
    static constexpr double EARTH_CIRCUMFERENCE = 40075016.68;
    static constexpr double HALF_EARTH_CIRCUMFERENCE = EARTH_CIRCUMFERENCE / 2;

    /// Construct an invalid tile.
    tile_t() noexcept = default;

    /**
     * Construct a new tile object.
     *
     * \pre \code zoom < 32 \endcode
     * \pre \code x < (1 << zoom) \endcode
     * \pre \code y < (1 << zoom) \endcode
     */
    tile_t(uint32_t zoom, uint32_t x, uint32_t y) noexcept
    : m_x(x), m_y(y), m_zoom(zoom)
    {
        assert(m_zoom < MAX_ZOOM);
        assert(x < (1UL << m_zoom));
        assert(y < (1UL << m_zoom));
    }

    uint32_t zoom() const noexcept
    {
        assert(valid());
        return m_zoom;
    }

    uint32_t x() const noexcept
    {
        assert(valid());
        return m_x;
    }

    uint32_t y() const noexcept
    {
        assert(valid());
        return m_y;
    }

    bool valid() const noexcept { return m_zoom != INVALID_ZOOM; }

    /// The width/height of the tile in web mercator (EPSG:3857) coordinates.
    double extent() const noexcept
    {
        return EARTH_CIRCUMFERENCE / static_cast<double>(1UL << m_zoom);
    }

    /// Minimum X coordinate of this tile in web mercator (EPSG:3857) units.
    double xmin() const noexcept
    {
        return -HALF_EARTH_CIRCUMFERENCE + m_x * extent();
    }

    /// Maximum X coordinate of this tile in web mercator (EPSG:3857) units.
    double xmax() const noexcept
    {
        return -HALF_EARTH_CIRCUMFERENCE + (m_x + 1) * extent();
    }

    /// Minimum Y coordinate of this tile in web mercator (EPSG:3857) units.
    double ymin() const noexcept
    {
        return HALF_EARTH_CIRCUMFERENCE - (m_y + 1) * extent();
    }

    /// Maximum Y coordinate of this tile in web mercator (EPSG:3857) units.
    double ymax() const noexcept
    {
        return HALF_EARTH_CIRCUMFERENCE - m_y * extent();
    }

    /// Same as box(margin).min_x().
    double xmin(double margin) const noexcept
    {
        return std::clamp(xmin() - margin * extent(), -HALF_EARTH_CIRCUMFERENCE,
                          HALF_EARTH_CIRCUMFERENCE);
    }

    /// Same as box(margin).max_x().
    double xmax(double margin) const noexcept
    {
        return std::clamp(xmax() + margin * extent(), -HALF_EARTH_CIRCUMFERENCE,
                          HALF_EARTH_CIRCUMFERENCE);
    }

    /// Same as box(margin).min_y().
    double ymin(double margin) const noexcept
    {
        return std::clamp(ymin() - margin * extent(), -HALF_EARTH_CIRCUMFERENCE,
                          HALF_EARTH_CIRCUMFERENCE);
    }

    /// Same as box(margin).max_y().
    double ymax(double margin) const noexcept
    {
        return std::clamp(ymax() + margin * extent(), -HALF_EARTH_CIRCUMFERENCE,
                          HALF_EARTH_CIRCUMFERENCE);
    }

    /**
     * Get bounding box from tile.
     */
    geom::box_t box() const noexcept
    {
        return {xmin(), ymin(), xmax(), ymax()};
    }

    /**
     * Get bounding box from tile with margin on all sides. The margin is
     * specified as a fraction of the tile extent. So margin==0.5 (50%) makes
     * the bounding box twice as wide and twice as heigh.
     *
     * The bounding box is clamped to the extent of the earth, so there will
     * be no coordinates smaller than -HALF_EARTH_CIRCUMFERENCE or larger than
     * HALF_EARTH_CIRCUMFERENCE.
     */
    geom::box_t box(double margin) const noexcept
    {
        return {xmin(margin), ymin(margin), xmax(margin), ymax(margin)};
    }

    /**
     * Convert a point from web mercator (EPSG:3857) coordinates to
     * coordinates in the tile assuming a tile extent of `pixel_extent`.
     */
    geom::point_t to_tile_coords(geom::point_t p,
                                 unsigned int pixel_extent) const noexcept;

    /**
     * Convert from tile coordinates (assuming a tile extent of `pixel_extent`
     * to web mercator (EPSG:3857) coordinates.
     */
    geom::point_t to_world_coords(geom::point_t p,
                                  unsigned int pixel_extent) const noexcept;

    /// The center of this tile in web mercator (EPSG:3857) units.
    geom::point_t center() const noexcept;

    /// Tiles are equal if x, y, and zoom are equal.
    friend bool operator==(tile_t const &a, tile_t const &b) noexcept
    {
        return (a.m_x == b.m_x) && (a.m_y == b.m_y) && (a.m_zoom == b.m_zoom);
    }

    friend bool operator!=(tile_t const &a, tile_t const &b) noexcept
    {
        return !(a == b);
    }

    /// Compare two tiles by the zoom, x, and y coordinates (in that order).
    friend bool operator<(tile_t const &a, tile_t const &b) noexcept
    {
        if (a.m_zoom < b.m_zoom) {
            return true;
        }
        if (a.m_zoom > b.m_zoom) {
            return false;
        }
        if (a.m_x < b.m_x) {
            return true;
        }
        if (a.m_x > b.m_x) {
            return false;
        }
        if (a.m_y < b.m_y) {
            return true;
        }

        return false;
    }

    /**
     * Return quadkey for this tile. The quadkey contains the interleaved
     * bits from the x and y values, similar to what's used for Bing maps:
     * https://docs.microsoft.com/en-us/bingmaps/articles/bing-maps-tile-system
     */
    quadkey_t quadkey() const noexcept;

    /**
     * Construct tile from quadkey.
     */
    static tile_t from_quadkey(quadkey_t quadkey, uint32_t zoom) noexcept;

private:
    static constexpr uint32_t INVALID_ZOOM =
        std::numeric_limits<uint32_t>::max();

    static constexpr uint32_t MAX_ZOOM = 32;

    uint32_t m_x = 0;
    uint32_t m_y = 0;
    uint32_t m_zoom = INVALID_ZOOM;
}; // class tile_t

/**
 * Iterate over tiles and call output function for each tile on all requested
 * zoom levels.
 *
 * \tparam OUTPUT Class with operator() taking a tile_t argument
 *
 * \param tiles_at_maxzoom The list of tiles at maximum zoom level
 * \param minzoom Minimum zoom level
 * \param maxzoom Maximum zoom level
 * \param output Output function
 */
template <class OUTPUT>
std::size_t for_each_tile(quadkey_list_t const &tiles_at_maxzoom,
                          uint32_t minzoom, uint32_t maxzoom,
                          OUTPUT const &output)
{
    assert(minzoom <= maxzoom);

    if (minzoom == maxzoom) {
        for (auto const quadkey : tiles_at_maxzoom) {
            output(tile_t::from_quadkey(quadkey, maxzoom));
        }
        return tiles_at_maxzoom.size();
    }

    /**
     * Loop over all requested zoom levels (from maximum down to the minimum
     * zoom level).
     */
    quadkey_t last_quadkey{};
    std::size_t count = 0;
    for (auto const quadkey : tiles_at_maxzoom) {
        for (uint32_t dz = 0; dz <= maxzoom - minzoom; ++dz) {
            auto const qt_current = quadkey.down(dz);
            /**
             * If dz > 0, there are probably multiple elements whose quadkey
             * is equal because they are all sub-tiles of the same tile at the
             * current zoom level. We skip all of them after we have written
             * the first sibling.
             */
            if (qt_current != last_quadkey.down(dz)) {
                output(tile_t::from_quadkey(qt_current, maxzoom - dz));
                ++count;
            }
        }
        last_quadkey = quadkey;
    }
    return count;
}

#endif // OSM2PGSQL_TILE_HPP
