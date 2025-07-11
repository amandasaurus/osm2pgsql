#ifndef OSM2PGSQL_OUTPUT_FLEX_HPP
#define OSM2PGSQL_OUTPUT_FLEX_HPP

/**
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * This file is part of osm2pgsql (https://osm2pgsql.org/).
 *
 * Copyright (C) 2006-2025 by the osm2pgsql developer community.
 * For a full list of authors see the git log.
 */

#include "expire-config.hpp"
#include "expire-output.hpp"
#include "expire-tiles.hpp"
#include "flex-table-column.hpp"
#include "flex-table.hpp"
#include "geom.hpp"
#include "idlist.hpp"
#include "locator.hpp"
#include "output.hpp"

#include <osmium/osm/item_type.hpp>

#include <lua.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class db_copy_thread_t;
class db_deleter_by_type_and_id_t;
class geom_transform_t;
class thread_pool_t;
struct options_t;

/**
 * When C++ code is called from the Lua code we sometimes need to know
 * in what context this happens. These are the possible contexts.
 */
enum class calling_context : std::uint8_t
{
    main = 0, ///< In main context, i.e. the Lua script outside any callbacks
    process_node = 1, ///< Inside a callback where a node is handled
    process_way = 2, ///< Inside a callback where a way is handled
    process_relation = 3, ///< Inside a callback where a relation is handled
    select_relation_members = 4 ///< In the select_relation_members() callback
};

/**
 * The flex output calls several user-defined Lua functions. They are
 * "prepared" by putting the function pointers on the Lua stack. Objects
 * of the class prepared_lua_function_t are used to hold the stack position
 * of the function which allows them to be called later using a symbolic
 * name.
 */
class prepared_lua_function_t
{
public:
    prepared_lua_function_t() noexcept = default;

    /**
     * Get function with the name "osm2pgsql.name" from Lua and put pointer
     * to it on the Lua stack.
     *
     * \param lua_state Current Lua state.
     * \param name Name of the function.
     * \param nresults The number of results this function is supposed to have.
     */
    prepared_lua_function_t(lua_State *lua_state, calling_context context,
                            char const *name, int nresults = 0);

    /// Return the index of the function on the Lua stack.
    int index() const noexcept { return m_index; }

    /// The name of the function.
    char const* name() const noexcept { return m_name; }

    /// The number of results this function is expected to have.
    int nresults() const noexcept { return m_nresults; }

    calling_context context() const noexcept { return m_calling_context; }

    /// Is this function defined in the users Lua code?
    explicit operator bool() const noexcept { return m_index != 0; }

private:
    char const *m_name = nullptr;
    int m_index = 0;
    int m_nresults = 0;
    calling_context m_calling_context = calling_context::main;
};

class output_flex_t : public output_t
{
public:
    /// Constructor for new objects
    output_flex_t(std::shared_ptr<middle_query_t> const &mid,
                  std::shared_ptr<thread_pool_t> thread_pool,
                  options_t const &options, properties_t const &properties);

    /// Constructor for cloned objects
    output_flex_t(output_flex_t const *other,
                  std::shared_ptr<middle_query_t> mid,
                  std::shared_ptr<db_copy_thread_t> copy_thread);

    output_flex_t(output_flex_t const &) = delete;
    output_flex_t &operator=(output_flex_t const &) = delete;

    output_flex_t(output_flex_t &&) = delete;
    output_flex_t &operator=(output_flex_t &&) = delete;

    ~output_flex_t() noexcept override = default;

    std::shared_ptr<output_t>
    clone(std::shared_ptr<middle_query_t> const &mid,
          std::shared_ptr<db_copy_thread_t> const &copy_thread) const override;

    void start() override;
    void stop() override;
    void sync() override;

    void after_nodes() override;
    void after_ways() override;
    void after_relations() override;

    void wait() override;

    idlist_t const &get_marked_node_ids() override;
    idlist_t const &get_marked_way_ids() override;

    void reprocess_marked() override;

    void pending_way(osmid_t id) override;
    void pending_relation(osmid_t id) override;
    void pending_relation_stage1c(osmid_t id) override;

    void select_relation_members(osmid_t id) override;

    void node_add(osmium::Node const &node) override;
    void way_add(osmium::Way *way) override;
    void relation_add(osmium::Relation const &rel) override;

    void node_modify(osmium::Node const &node) override;
    void way_modify(osmium::Way *way) override;
    void relation_modify(osmium::Relation const &rel) override;

    void node_delete(osmium::Node const &node) override;
    void way_delete(osmium::Way *way) override;
    void relation_delete(osmium::Relation const &rel) override;

    void merge_expire_trees(output_t *other) override;

    int app_as_point();
    int app_as_linestring();
    int app_as_polygon();
    int app_as_multipoint();
    int app_as_multilinestring();
    int app_as_multipolygon();
    int app_as_geometrycollection();

    int app_define_locator();
    int app_define_table();
    int app_define_expire_output();
    int app_get_bbox();

    int table_insert();

    // Get the flex table that is as first parameter on the Lua stack.
    flex_table_t &get_table_from_param();

    // Get the expire output that is as first parameter on the Lua stack.
    expire_output_t &get_expire_output_from_param();

    // Get the flex locator that is as first parameter on the Lua stack.
    locator_t &get_locator_from_param();

private:
    void select_relation_members();

    /// Call a Lua function that was "prepared" earlier.
    void call_lua_function(prepared_lua_function_t func);

    /**
     * Call a Lua function that was "prepared" earlier with the OSMObject
     * as its only parameter.
     */
    void call_lua_function(prepared_lua_function_t func,
                           osmium::OSMObject const &object);

    /// Aquire the lua_mutex and then call `call_lua_function()`.
    void get_mutex_and_call_lua_function(prepared_lua_function_t func);

    void get_mutex_and_call_lua_function(prepared_lua_function_t func,
                                         osmium::OSMObject const &object);

    void init_lua(std::string const &filename, properties_t const &properties);

    void check_context_and_state(char const *name, char const *context,
                                 bool condition);

    osmium::OSMObject const &
    check_and_get_context_object(flex_table_t const &table);

    void node_delete(osmid_t id);
    void way_delete(osmid_t id);
    void relation_delete(osmid_t id);

    void delete_from_table(table_connection_t *table_connection,
                           pg_conn_t const &db_connection,
                           osmium::item_type type, osmid_t osm_id);

    void delete_from_tables(osmium::item_type type, osmid_t osm_id);

    lua_State *lua_state() noexcept { return m_lua_state.get(); }

    class way_cache_t
    {
    public:
        bool init(middle_query_t const &middle, osmid_t id);
        void init(osmium::Way *way);
        std::size_t add_nodes(middle_query_t const &middle);
        osmium::Way const &get() const noexcept { return *m_way; }

    private:
        osmium::memory::Buffer m_buffer{32768,
                                        osmium::memory::Buffer::auto_grow::yes};
        osmium::Way *m_way = nullptr;
        std::size_t m_num_way_nodes = std::numeric_limits<std::size_t>::max();

    }; // way_cache_t

    class relation_cache_t
    {
    public:
        bool init(middle_query_t const &middle, osmid_t id);
        void init(osmium::Relation const &relation);

        /**
         * Add members of relation to cache when it is first called.
         *
         * \returns True if at least one member was found, false otherwise
         */
        bool add_members(middle_query_t const &middle);

        osmium::Relation const &get() const noexcept
        {
            return *m_relation;
        }

        osmium::memory::Buffer const &members_buffer() const noexcept
        {
            return m_members_buffer;
        }

    private:
        // This buffer is used for the relation only. Members are stored in
        // m_members_buffer. If we would store more objects in this buffer,
        // it might autogrow which would invalidate the m_relation pointer.
        osmium::memory::Buffer m_relation_buffer{
            1024, osmium::memory::Buffer::auto_grow::yes};
        osmium::memory::Buffer m_members_buffer{
            32768, osmium::memory::Buffer::auto_grow::yes};
        osmium::Relation const *m_relation = nullptr;

    }; // relation_cache_t

    std::shared_ptr<std::vector<locator_t>> m_locators =
        std::make_shared<std::vector<locator_t>>();

    std::shared_ptr<std::vector<flex_table_t>> m_tables =
        std::make_shared<std::vector<flex_table_t>>();

    std::shared_ptr<std::vector<expire_output_t>> m_expire_outputs =
        std::make_shared<std::vector<expire_output_t>>();

    std::vector<table_connection_t> m_table_connections;

    /// The connection to the database server.
    pg_conn_t m_db_connection;

    // These are shared between all clones of the output and must only be
    // accessed while protected using the lua_mutex.
    std::shared_ptr<idlist_t> m_stage2_node_ids = std::make_shared<idlist_t>();
    std::shared_ptr<idlist_t> m_stage2_way_ids = std::make_shared<idlist_t>();

    std::shared_ptr<db_copy_thread_t> m_copy_thread;

    // This is shared between all clones of the output and must only be
    // accessed while protected using the lua_mutex.
    std::shared_ptr<lua_State> m_lua_state;

    std::vector<expire_tiles> m_expire_tiles;

    way_cache_t m_way_cache;
    relation_cache_t m_relation_cache;
    osmium::Node const *m_context_node = nullptr;

    osmium::memory::Buffer m_area_buffer;

    prepared_lua_function_t m_process_node;
    prepared_lua_function_t m_process_way;
    prepared_lua_function_t m_process_relation;

    prepared_lua_function_t m_process_untagged_node;
    prepared_lua_function_t m_process_untagged_way;
    prepared_lua_function_t m_process_untagged_relation;

    prepared_lua_function_t m_process_deleted_node;
    prepared_lua_function_t m_process_deleted_way;
    prepared_lua_function_t m_process_deleted_relation;

    prepared_lua_function_t m_select_relation_members;

    prepared_lua_function_t m_after_nodes;
    prepared_lua_function_t m_after_ways;
    prepared_lua_function_t m_after_relations;

    calling_context m_calling_context = calling_context::main;

    /**
     * This is set before calling stage1c process_relation() to disable the
     * insert() command.
     */
    bool m_disable_insert = false;
};

int lua_trampoline_table_insert(lua_State *lua_state);

#endif // OSM2PGSQL_OUTPUT_FLEX_HPP
