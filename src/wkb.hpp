#ifndef OSM2PGSQL_WKB_HPP
#define OSM2PGSQL_WKB_HPP

/**
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This file is part of osm2pgsql (https://osm2pgsql.org/).
 *
 * Copyright (C) 2006-2025 by the osm2pgsql developer community.
 * For a full list of authors see the git log.
 */

#include "geom.hpp"

#include <string>
#include <string_view>

/**
 * \file
 *
 * Functions for converting geometries from and to (E)WKB.
 */

/**
 * Convert single geometry to EWKB
 *
 * \param geom Input geometry
 * \param ensure_multi Wrap non-multi geometries in multi geometries
 * \returns String with EWKB encoded geometry
 */
[[nodiscard]] std::string geom_to_ewkb(geom::geometry_t const &geom,
                                       bool ensure_multi = false);

/**
 * Convert EWKB geometry to geometry object. If the input is empty, a null
 * geometry is returned. If the WKB can not be parsed, an exception is thrown.
 *
 * \param wkb Input EWKB geometry in binary format
 * \returns Geometry
 */
[[nodiscard]] geom::geometry_t ewkb_to_geom(std::string_view wkb);

#endif // OSM2PGSQL_WKB_HPP
