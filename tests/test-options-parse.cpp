/**
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This file is part of osm2pgsql (https://osm2pgsql.org/).
 *
 * Copyright (C) 2006-2025 by the osm2pgsql developer community.
 * For a full list of authors see the git log.
 */

#include <catch.hpp>

#include <vector>

#include "command-line-parser.hpp"
#include "taginfo-impl.hpp"
#include "tagtransform.hpp"

namespace {

char const *const TEST_PBF = "foo.pbf";

void bad_opt(std::vector<char const *> opts, char const *msg)
{
    opts.insert(opts.cbegin(), "osm2pgsql");
    opts.push_back(TEST_PBF);

    REQUIRE_THROWS_WITH(
        parse_command_line((int)opts.size(), (char **)opts.data()),
        Catch::Matchers::Contains(msg));
}

options_t opt(std::vector<char const *> opts)
{
    opts.insert(opts.cbegin(), "osm2pgsql");
    opts.push_back(TEST_PBF);

    return parse_command_line((int)opts.size(), (char **)opts.data());
}

} // anonymous namespace

TEST_CASE("Insufficient arguments", "[NoDB]")
{
    std::vector<char const *> opts = {"osm2pgsql", "-c", "--slim"};

    REQUIRE_THROWS_WITH(
        parse_command_line((int)opts.size(), (char **)opts.data()),
        Catch::Matchers::Contains("Missing input"));
}

TEST_CASE("Incompatible arguments", "[NoDB]")
{
    bad_opt({"-a", "-c", "--slim"}, "options can not be used at the same time");

    bad_opt({"-j", "-k"}, "--hstore excludes --hstore-all");

    bad_opt({"-a"}, "--append can only be used with slim mode");
}

TEST_CASE("Middle selection", "[NoDB]")
{
    auto options = opt({"--slim"});
    REQUIRE(options.slim);

    options = opt({});
    REQUIRE_FALSE(options.slim);
}

TEST_CASE("Lua styles", "[NoDB]")
{
    REQUIRE_THROWS_WITH(opt({"--tag-transform-script", "non_existing.lua"}),
                        Catch::Matchers::Contains("File does not exist"));
}

TEST_CASE("Parsing bbox", "[NoDB]")
{
    auto const opt1 = opt({"-b", "1.2,3.4,5.6,7.8"});
    CHECK(opt1.bbox == osmium::Box{1.2, 3.4, 5.6, 7.8});

    auto const opt2 = opt({"--bbox", "1.2,3.4,5.6,7.8"});
    CHECK(opt2.bbox == osmium::Box{1.2, 3.4, 5.6, 7.8});

    auto const opt3 = opt({"--bbox", "1.2, 3.4, 5.6, 7.8"});
    CHECK(opt3.bbox == osmium::Box{1.2, 3.4, 5.6, 7.8});
}

TEST_CASE("Parsing bbox fails if coordinates in wrong order", "[NoDB]")
{
    bad_opt({"--bbox", "1.0,2.0,0.0,0.0"}, "Bounding box failed due to");
}

TEST_CASE("Parsing bbox fails if wrong format", "[NoDB]")
{
    bad_opt({"-b", "123"}, "Bounding box must be specified like:"
                           " minlon,minlat,maxlon,maxlat.");

    bad_opt({"-b", "1,2,3,4x"}, "Bounding box must be specified like:"
                                " minlon,minlat,maxlon,maxlat.");

    bad_opt({"-b", "1,,3,4"}, "Bounding box must be specified like:"
                              " minlon,minlat,maxlon,maxlat.");

    bad_opt({"-b", "1,2,3"}, "Bounding box must be specified like:"
                             " minlon,minlat,maxlon,maxlat.");

    bad_opt({"-b", "1,2,3,4,5"}, "Bounding box must be specified like:"
                                 " minlon,minlat,maxlon,maxlat.");

    bad_opt({"-b", "1,2,INF,4"}, "Bounding box must be specified like:"
                                 " minlon,minlat,maxlon,maxlat.");

    bad_opt({"-b", "1,NAN,3,4"}, "Bounding box must be specified like:"
                                 " minlon,minlat,maxlon,maxlat.");
}

TEST_CASE("Parsing number-processes", "[NoDB]")
{
    auto const opt1 = opt({"--number-processes", "0"});
    CHECK(opt1.num_procs == 1);

    auto const opt2 = opt({"--number-processes", "1"});
    CHECK(opt2.num_procs == 1);

    auto const opt3 = opt({"--number-processes", "2"});
    CHECK(opt3.num_procs == 2);

    auto const opt4 = opt({"--number-processes", "32"});
    CHECK(opt4.num_procs == 32);

    auto const opt5 = opt({"--number-processes", "33"});
    CHECK(opt5.num_procs == 32);
}

TEST_CASE("Parsing tile expiry zoom levels", "[NoDB]")
{
    auto options = opt({"-e", "8-12"});
    CHECK(options.expire_tiles_zoom_min == 8);
    CHECK(options.expire_tiles_zoom == 12);

    options = opt({"-e", "12"});
    CHECK(options.expire_tiles_zoom_min == 12);
    CHECK(options.expire_tiles_zoom == 12);

    //  If very high zoom levels are set, back to still high but valid zoom levels.
    options = opt({"-e", "33-35"});
    CHECK(options.expire_tiles_zoom_min == 31);
    CHECK(options.expire_tiles_zoom == 31);
}

TEST_CASE("Parsing tile expiry zoom levels fails", "[NoDB]")
{
    bad_opt({"-e", "8--12"},
            "Invalid maximum zoom level given for tile expiry");

    bad_opt({"-e", "-8-12"}, "Missing argument for option --expire-tiles. Zoom "
                             "levels must be positive.");

    bad_opt({"-e", "--style", "default.style"},
            "Missing argument for option --expire-tiles. Zoom levels must be "
            "positive.");

    bad_opt({"-e", "a-8"}, "Bad argument for option --expire-tiles. Minimum "
                           "zoom level must be larger than 0.");

    bad_opt({"-e", "6:8"}, "Minimum and maximum zoom level for tile expiry "
                           "must be separated by '-'.");

    bad_opt({"-e", "6-0"}, "Invalid maximum zoom level given for tile expiry.");

    bad_opt({"-e", "6-9a"},
            "Invalid maximum zoom level given for tile expiry.");

    bad_opt({"-e", "0-8"},
            "Bad argument for option --expire-tiles. Minimum zoom level "
            "must be larger than 0.");

    bad_opt({"-e", "6-"}, "Invalid maximum zoom level given for tile expiry.");

    bad_opt({"-e", "-6"},
            "Missing argument for option --expire-tiles. Zoom levels "
            "must be positive.");

    bad_opt({"-e", "0"},
            "Bad argument for option --expire-tiles. Minimum zoom level "
            "must be larger than 0.");
}

TEST_CASE("Parsing log-level", "[NoDB]")
{
    opt({"--log-level", "debug"});
    opt({"--log-level", "info"});
    opt({"--log-level", "warn"});
    opt({"--log-level", "warning"});
    opt({"--log-level", "error"});
}

TEST_CASE("Parsing log-level fails for unknown level", "[NoDB]")
{
    bad_opt({"--log-level", "foo"}, "--log-level: foo not in");
}

TEST_CASE("Parsing log-progress", "[NoDB]")
{
    opt({"--log-progress", "true"});
    opt({"--log-progress", "false"});
    opt({"--log-progress", "auto"});
}

TEST_CASE("Parsing log-progress fails for unknown value", "[NoDB]")
{
    bad_opt({"--log-progress", "foo"},
            "Unknown value for --log-progress option: ");
}
