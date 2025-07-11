#ifndef OSM2PGSQL_TESTS_COMMON_IMPORT_HPP
#define OSM2PGSQL_TESTS_COMMON_IMPORT_HPP

/**
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This file is part of osm2pgsql (https://osm2pgsql.org/).
 *
 * Copyright (C) 2006-2025 by the osm2pgsql developer community.
 * For a full list of authors see the git log.
 */

#include <osmium/handler.hpp>
#include <osmium/io/any_input.hpp>
#include <osmium/io/file.hpp>
#include <osmium/io/reader.hpp>
#include <osmium/osm/types_from_string.hpp>
#include <osmium/visitor.hpp>

#include "input.hpp"
#include "middle-pgsql.hpp"
#include "middle-ram.hpp"
#include "osmdata.hpp"
#include "output.hpp"
#include "properties.hpp"
#include "taginfo-impl.hpp"

#include "common-pg.hpp"

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

namespace testing {

inline void parse_file(options_t const &options,
                       std::shared_ptr<middle_t> const &mid,
                       std::shared_ptr<output_t> const &output,
                       char const *filename = nullptr, bool do_stop = true)
{
    osmdata_t osmdata{mid, output, options};

    std::string filepath{TESTDATA_DIR};
    if (filename) {
        filepath += filename;
    } else {
        filepath += options.input_files[0];
    }
    osmium::io::File const file{filepath};
    process_files({file}, &osmdata, options.append, false);

    if (do_stop) {
        osmdata.stop();
    }
}

namespace db {

/**
 * This is used as a helper to assemble OSM objects into an OPL file which
 * can later be used as input for testing.
 */
class data_t
{
public:
    data_t() = default;

    template <typename CONTAINER>
    explicit data_t(CONTAINER const &objects)
    {
        std::copy(std::cbegin(objects), std::cend(objects),
                  std::back_inserter(m_objects));
    }

    void add(char const *object) { m_objects.emplace_back(object); }

    template <typename CONTAINER>
    void add(CONTAINER const &objects)
    {
        std::copy(std::cbegin(objects), std::cend(objects),
                  std::back_inserter(m_objects));
    }

    void add(std::initializer_list<char const *> const &objects)
    {
        std::copy(std::cbegin(objects), std::cend(objects),
                  std::back_inserter(m_objects));
    }

    char const *operator()()
    {
        std::sort(m_objects.begin(), m_objects.end(),
                  [](std::string const &a, std::string const &b) {
                      return get_type_id(a) < get_type_id(b);
                  });

        m_result.clear();
        for (auto const &obj : m_objects) {
            assert(!obj.empty());
            m_result.append(obj);
            if (m_result.back() != '\n') {
                m_result += '\n';
            }
        }

        return m_result.c_str();
    }

private:
    static std::pair<osmium::item_type, osmium::object_id_type>
    get_type_id(std::string const &str)
    {
        std::string ti(str, 0, str.find(' '));
        return osmium::string_to_object_id(ti.c_str(),
                                           osmium::osm_entity_bits::nwr);
    }

    std::vector<std::string> m_objects;
    std::string m_result;
};

/**
 * Convenience class around tempdb_t that offers functions for
 * data import from file and strings.
 */
class import_t
{
public:
    void run_import(options_t options,
                    std::initializer_list<std::string> input_data,
                    std::string const &format = "opl")
    {
        options.connection_params.merge_with(m_db.connection_params());

        properties_t const properties{options.connection_params,
                                      options.middle_dbschema};

        auto thread_pool = std::make_shared<thread_pool_t>(1U);
        auto middle = create_middle(thread_pool, options);
        middle->start();

        auto output = output_t::create_output(middle->get_query_instance(),
                                              thread_pool, options, properties);

        middle->set_requirements(output->get_requirements());

        osmdata_t osmdata{middle, output, options};

        std::vector<osmium::io::File> files;
        for (auto const &data : input_data) {
            files.emplace_back(data.data(), data.size(), format);
        }
        process_files(files, &osmdata, options.append, false);

        osmdata.stop();
    }

    void run_import(options_t options, char const *data,
                    std::string const &format = "opl")
    {
        run_import(std::move(options), std::initializer_list<std::string>{data},
                   format);
    }

    void run_file(options_t options, char const *file = nullptr)
    {
        options.connection_params.merge_with(m_db.connection_params());

        properties_t const properties{options.connection_params,
                                      options.middle_dbschema};

        auto thread_pool = std::make_shared<thread_pool_t>(1U);
        auto middle = std::make_shared<middle_ram_t>(thread_pool, &options);
        middle->start();

        auto output = output_t::create_output(middle->get_query_instance(),
                                              thread_pool, options, properties);

        middle->set_requirements(output->get_requirements());

        parse_file(options, middle, output, file);

        middle->wait();
        output->wait();
    }

    testing::pg::conn_t connect() { return m_db.connect(); }

    testing::pg::tempdb_t const &db() const noexcept { return m_db; }

private:
    testing::pg::tempdb_t m_db;
};

} // namespace db
} // namespace testing

#endif // OSM2PGSQL_TESTS_COMMON_IMPORT_HPP
