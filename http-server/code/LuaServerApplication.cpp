
/// @copyright Copyright (c) 2026 Ángel, All rights reserved.
/// angel.rodriguez@udit.es

#include "LuaServerApplication.hpp"

namespace argb
{

    bool LuaServerApplication::RequestHandler::process (const HttpRequest & request, HttpResponse & response)
    {
        try
        {
            HttpResponse::Serializer        response_serializer(response);
            LuaHttpResponseSerializerBridge response_bridge    (response_serializer);

            auto request_table  = server.virtual_machine.newTable ();
            auto response_table = server.virtual_machine.newTable ();

            request_table .set ("__native_object_pointer", static_cast<lua::Pointer>(const_cast<HttpRequest *>(&request)));
            response_table.set ("__native_object_pointer", static_cast<lua::Pointer>(&response_bridge));

            server.virtual_machine["setmetatable"]
            (
                request_table,
                server.virtual_machine["__http_request_metatable"]
            );

            server.virtual_machine["setmetatable"]
            (
                response_table,
                server.virtual_machine["__http_response_metatable"]
            );

            endpoint.unref ().call (request_table, response_table);
        }
        catch (const lua::RuntimeError & )
        {
            send_plain_text_response (response, 500, "Internal Server Error");
        }

        return true;
    }

    LuaServerApplication::LuaServerApplication(const std::string_view & script_path_string)
    {
        std::filesystem::path script_path(script_path_string);

        // Check if the script file exists and is a regular file:

        if (not std::filesystem::exists (script_path) || not std::filesystem::is_regular_file (script_path))
        {
            throw std::runtime_error("Script file does not exist or cannot be accessed or excecuted: " + script_path.string ());
        }

        // Set the base path to the directory containing the script file:

        base_path = std::filesystem::absolute (script_path).parent_path ();

        // Create the bridge between Lua and C++:

        create_lua_bridge ();

        // Load the starting Lua script:

        virtual_machine.doFile (script_path.string ());
    }

    HttpRequestHandler::Ptr LuaServerApplication::create_handler (HttpRequest::Method method, std::string_view path)
    {
        auto method_index = static_cast<int>(method);

        if (method_index >= 0 && method_index < static_cast<int>(HttpRequest::Method::COUNT))
        {
            auto & endpoints_by_path = endpoints[method_index];

            auto iterator = endpoints_by_path.upper_bound (path);

            if (iterator != endpoints_by_path.begin ())
            {
                --iterator;

                const auto & key = iterator->first;

                if (path.starts_with (key) && (path.length () == key.length () || path[key.length ()] == '/' || key.back () == '/'))
                {
                    return { std::make_unique<RequestHandler> (*this, iterator->second) };
                }
            }
        }

        return {};
    }

    void LuaServerApplication::create_lua_bridge ()
    {
        create_bridge_for_server        ();
        create_bridge_for_http_request  ();
        create_bridge_for_http_response ();
        create_bridge_for_sqlite        ();
    }

    void LuaServerApplication::create_bridge_for_server ()
    {
        auto server_bridge = virtual_machine.newTable ();

        server_bridge.set
        (
            "route",
            [this](lua::Value method_value, lua::Value path_value, lua::Value endpoint_value)
            {
                if (not method_value.is<lua::String>())
                {
                    virtual_machine.error ("Parameter 'method' must be a string.");
                }

                if (not path_value.is<lua::String>())
                {
                    virtual_machine.error ("Parameter 'path' must be a string.");
                }

                bridge_server_route (method_value.toString (), path_value.toString (), endpoint_value);
            }
        );

        virtual_machine.set ("server", server_bridge);
    }

    void LuaServerApplication::create_bridge_for_http_request ()
    {
        auto http_request_bridge = virtual_machine.newTable ();

        create_method_bridge (http_request_bridge, "get_protocol", &HttpRequest::get_protocol);
        create_method_bridge (http_request_bridge, "get_method",   &HttpRequest::get_method  );
        create_method_bridge (http_request_bridge, "get_path",     &HttpRequest::get_path    );
        create_method_bridge (http_request_bridge, "get_fragment", &HttpRequest::get_fragment);
        create_method_bridge (http_request_bridge, "get_header",   &HttpRequest::get_header  );
        create_method_bridge (http_request_bridge, "get_query",    &HttpRequest::get_query   );
        create_method_bridge (http_request_bridge, "get_body",     &HttpRequest::get_body    );

        auto http_request_metatable = virtual_machine.newTable ();

        http_request_metatable.set ("__index",      http_request_bridge);
        http_request_metatable.set ("__metatable", "protected");

        http_request_metatable.set
        (
            "__newindex",
            [this](lua::Value, lua::Value, lua::Value)
            {
                virtual_machine.error("HTTP request is read-only.");
            }
        );

        virtual_machine.set ("__http_request_metatable", http_request_metatable);
    }

    void LuaServerApplication::create_bridge_for_http_response ()
    {
        auto http_response_bridge = virtual_machine.newTable ();

        create_method_bridge (http_response_bridge, "status",     &LuaHttpResponseSerializerBridge::status    );
        create_method_bridge (http_response_bridge, "header",     &LuaHttpResponseSerializerBridge::header    );
        create_method_bridge (http_response_bridge, "end_header", &LuaHttpResponseSerializerBridge::end_header);
        create_method_bridge (http_response_bridge, "body",       &LuaHttpResponseSerializerBridge::body      );

        auto http_response_metatable = virtual_machine.newTable ();

        http_response_metatable.set ("__index",      http_response_bridge);
        http_response_metatable.set ("__metatable", "protected");

        http_response_metatable.set
        (
            "__newindex",
            [this] (lua::Value, lua::Value, lua::Value)
            {
                virtual_machine.error ("HTTP response fields cannot be assigned directly.");
            }
        );

        virtual_machine.set ("__http_response_metatable", http_response_metatable);
    }

    void LuaServerApplication::bridge_server_route (const std::string & method_string, const std::string & path, lua::Value endpoint)
    {
        if (path.empty () || path.front () != '/')
        {
            virtual_machine.error ("Parameter 'path' must be a non-empty string starting with '/'.");
        }

        if (not endpoint.is<lua::Callable> ())
        {
            virtual_machine.error ("Parameter 'endpoint' must be a Lua function.");
        }

        auto method = HttpRequest::Parser::method_from_string (method_string);

        if (method == HttpRequest::Method::UNDEFINED)
        {
            virtual_machine.error ("Invalid HTTP method: %s.", method_string.c_str ());
        }

        auto & endpoints_by_path = endpoints[static_cast<size_t>(method)];

        // Keys ending with '/' act as prefix routes (e.g. "/users/" matches "/users/1").
        // Keys without a trailing '/' match exactly or as a path-segment prefix.
        // Both forms are kept as-is so they coexist as distinct entries in the map.

        endpoints_by_path.insert_or_assign (std::string(path), std::move(endpoint));
    }

    static std::vector<Sqlite::SqlValue> collect_sql_args (lua::Value & args)
    {
        std::vector<Sqlite::SqlValue> result;

        if (args.is<lua::Table> ())
        {
            for (int i = 1; ; ++i)
            {
                lua::Value arg = args[i];

                if      (arg.is<lua::Number> ()) result.emplace_back (arg.to<double>     ());
                else if (arg.is<lua::String> ()) result.emplace_back (arg.to<std::string>());
                else break;
            }
        }

        return result;
    }

    void LuaServerApplication::create_bridge_for_sqlite ()
    {
        auto db_table = virtual_machine.newTable ();

        db_table.set
        (
            "execute",
            [this] (std::string sql, lua::Value args)
            {
                auto bind_args = collect_sql_args (args);

                database ().execute_all (sql, bind_args);
            }
        );

        db_table.set
        (
            "query",
            [this] (std::string sql, lua::Value args) -> lua::Value
            {
                auto bind_args = collect_sql_args (args);

                return make_sqlite_row_table (database ().query_all (sql, bind_args));
            }
        );

        virtual_machine.set ("db", db_table);
    }

    lua::Value LuaServerApplication::make_sqlite_row_table (Sqlite::Row row)
    {
        auto  bridge     = std::make_unique<LuaSqliteRowBridge> (std::move (row));
        auto  bridge_ptr = bridge.get ();

        database_row_bridges.emplace (bridge_ptr, std::move (bridge));

        auto row_table = virtual_machine.newTable ();

        row_table.set ("__native_object_pointer", static_cast<lua::Pointer>(bridge_ptr));

        // Plant a sentinel lambda inside the row table. When Lua GCs the row table, this functor
        // userdata becomes unreachable and its __gc metamethod fires, destroying the lambda and
        // invoking the custom deleter, which removes the bridge from the map:

        auto gc_hook = std::shared_ptr<void>
        (
            bridge_ptr,
            [this] (LuaSqliteRowBridge * key) { database_row_bridges.erase (key); }
        );

        row_table.set ("__gc_sentinel", [gc_hook] () {});

        auto row_method_table = virtual_machine.newTable ();

        create_method_bridge (row_method_table, "advance",     &LuaSqliteRowBridge::advance    );
        create_method_bridge (row_method_table, "get_integer", &LuaSqliteRowBridge::get_integer);
        create_method_bridge (row_method_table, "get_string",  &LuaSqliteRowBridge::get_string );
        create_method_bridge (row_method_table, "get_real",    &LuaSqliteRowBridge::get_real   );

        auto row_metatable = virtual_machine.newTable ();

        row_metatable.set ("__index",     row_method_table);
        row_metatable.set ("__metatable", "protected"     );

        row_metatable.set
        (
            "__newindex",
            [this] (lua::Value, lua::Value, lua::Value)
            {
                virtual_machine.error ("Row fields cannot be assigned directly.");
            }
        );

        virtual_machine["setmetatable"] (row_table, row_metatable);

        return row_table;
    }

}
