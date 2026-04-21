/// @copyright Copyright (c) 2026 Ángel, All rights reserved.
/// angel.rodriguez@udit.es

#pragma once

#include <array>
#include <filesystem>
#include <HttpRequestHandler.hpp>
#include <HttpRequestHandlerFactory.hpp>
#include <HttpResponse.hpp>
#include "LuaTypeAdapter.hpp"
#include <map>
#include <memory>
#include "Sqlite.hpp"
#include <type_traits>
#include <vector>

namespace argb
{

    class LuaServerApplication : public HttpRequestHandlerFactory
    {

        using VirtualMachine           = lua::State;
        using Database                 = Sqlite;
        using DatabasePtr              = std::unique_ptr<Database>;
        using Endpoint                 = lua::Ref;
        using EndpointsByPath          = std::map<std::string, Endpoint, std::less<>>;
        using EndpointsByMethodAndPath = std::array<EndpointsByPath, static_cast<size_t>(HttpRequest::Method::COUNT)>;

        /** This class implements the logic for handling HTTP requests that are routed to Lua-defined endpoints. It
          * manages the state of the request processing, including invoking the appropriate Lua function for the
          * requested endpoint and generating appropriate HTTP responses based on the outcome of these invocations.
          */
        class RequestHandler : public HttpRequestHandler
        {
            LuaServerApplication & server;
            Endpoint & endpoint;

        public:

            /** Constructs a RequestHandler for the specified LuaServerApplication and endpoint. It initializes the
              * handler with references to the server and the Lua endpoint that it will invoke when processing requests.
              */
            RequestHandler(LuaServerApplication & server, Endpoint & endpoint)
                : server  (server  )
                , endpoint(endpoint)
            {
            }

            /** Processes the incoming HTTP request and generates an appropriate response by invoking the corresponding
              * Lua function for the requested endpoint. It handles any exceptions that may occur during the Lua function
              * invocation and generates a generic error response if necessary.
              * The method returns true when the response is fully ready to be sent back to the client, or false if the
              * handler is still processing (e.g., waiting for Lua function execution).
              * @param request The incoming HTTP request that triggered this handler.
              * @param response The HTTP response object that will be populated with the appropriate status, headers,
              *     and body based on the processing of the request and the outcome of the Lua function invocation.
              * @return true if the response is fully ready to be sent back to the client, or false if the handler is
              *     still processing (e.g., waiting for Lua function execution).
              */
            bool process (const HttpRequest & request, HttpResponse & response) override;

        };

        class LuaHttpResponseSerializerBridge
        {
            HttpResponse::Serializer * serializer;

        public:

            LuaHttpResponseSerializerBridge(HttpResponse::Serializer & serializer)
                : serializer(&serializer)
            {
            }

            void status (int value)
            {
                serializer->status (value);
            }

            void header (const std::string & name, const std::string & value)
            {
                serializer->header (name, value);
            }

            void end_header ()
            {
                serializer->end_header ();
            }

            void body (const std::string & content)
            {
                serializer->body (std::span<const char>(content.data (), content.size ()));
            }
        };

        class LuaSqliteRowBridge
        {
            Sqlite::Row row;

        public:

            LuaSqliteRowBridge(Sqlite::Row row)
                : row(std::move(row))
            {
            }

            bool        advance     ()          { return row.advance ();                   }
            int         get_integer (int index) { return row.get<int>         (index - 1); }
            std::string get_string  (int index) { return row.get<std::string> (index - 1); }
            double      get_real    (int index) { return row.get<double>      (index - 1); }
        };

        using DatabaseRowBridges = std::map<LuaSqliteRowBridge *, std::unique_ptr<LuaSqliteRowBridge>>;

    private:

        DatabasePtr              database_ptr;
        DatabaseRowBridges       database_row_bridges;
        VirtualMachine           virtual_machine;
        EndpointsByMethodAndPath endpoints;
        std::filesystem::path    base_path;

    public:

        LuaServerApplication(const std::string_view & script_path_string);

        HttpRequestHandler::Ptr create_handler (HttpRequest::Method method, std::string_view path) override;

    private:

        Database & database ()
        {
            if (not database_ptr) database_ptr = std::make_unique<Database> (base_path / "database.bin");
            return *database_ptr;
        }

        void create_lua_bridge ();

        void create_bridge_for_server        ();
        void create_bridge_for_http_request  ();
        void create_bridge_for_http_response ();
        void create_bridge_for_sqlite        ();

        lua::Value make_sqlite_row_table (Sqlite::Row row);

        void bridge_server_route (const std::string & method_string, const std::string & path_string, lua::Value endpoint);

    private:

        template<class CLASS_TYPE>
        CLASS_TYPE * get_native_object_pointer (lua::Value table);

        template<class CLASS_TYPE, typename RETURN_TYPE, typename ... ARGUMENTS>
        void create_method_bridge
        (
            lua::Value & table,
            const std::string & method_name,
            RETURN_TYPE (CLASS_TYPE::*method_pointer) (ARGUMENTS...)
        );

        template<class CLASS_TYPE, typename RETURN_TYPE, typename ... ARGUMENTS>
        void create_method_bridge
        (
            lua::Value & table,
            const std::string & method_name,
            RETURN_TYPE (CLASS_TYPE::*method_pointer) (ARGUMENTS...) const
        );

    };

    template<class CLASS_TYPE>
    CLASS_TYPE * LuaServerApplication::get_native_object_pointer (lua::Value table)
    {
        auto pointer = table["__native_object_pointer"].to<lua::Pointer> ();

        if (pointer == nullptr)
        {
            virtual_machine.error ("Invalid native_object.");
        }

        return static_cast<CLASS_TYPE *>(pointer);
    }

    template<class CLASS_TYPE, typename RETURN_TYPE, typename ... ARGUMENTS>
    void LuaServerApplication::create_method_bridge
    (
        lua::Value & table,
        const std::string & method_name,
        RETURN_TYPE (CLASS_TYPE::*method_pointer) (ARGUMENTS...)
    )
    {
        table.set
        (
            method_name,
            [this, method_pointer] (lua::Value self, typename LuaTypeAdapter<ARGUMENTS>::LuaType ... arguments)
            {
                auto * object = get_native_object_pointer<CLASS_TYPE> (self);

                if constexpr (std::is_void_v<RETURN_TYPE>)
                {
                    (object->*method_pointer) (LuaTypeAdapter<ARGUMENTS>::adapt_from_lua (arguments)...);
                }
                else
                {
                    return LuaTypeAdapter<RETURN_TYPE>::adapt_for_lua
                    (
                        (object->*method_pointer) (LuaTypeAdapter<ARGUMENTS>::adapt_from_lua (arguments)...)
                    );
                }
            }
        );
    }

    template<class CLASS_TYPE, typename RETURN_TYPE, typename ... ARGUMENTS>
    void LuaServerApplication::create_method_bridge
    (
        lua::Value & table,
        const std::string & method_name,
        RETURN_TYPE (CLASS_TYPE::*method_pointer) (ARGUMENTS...) const
    )
    {
        table.set
        (
            method_name,
            [this, method_pointer] (lua::Value self, typename LuaTypeAdapter<ARGUMENTS>::LuaType ... arguments)
            {
                auto * object = get_native_object_pointer<CLASS_TYPE> (self);

                if constexpr (std::is_void_v<RETURN_TYPE>)
                {
                    (object->*method_pointer) (LuaTypeAdapter<ARGUMENTS>::adapt_from_lua (arguments)...);
                }
                else
                {
                    return LuaTypeAdapter<RETURN_TYPE>::adapt_for_lua
                    (
                        (object->*method_pointer) (LuaTypeAdapter<ARGUMENTS>::adapt_from_lua (arguments)...)
                    );
                }
            }
        );
    }

}
