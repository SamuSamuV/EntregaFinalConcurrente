
/// @copyright Copyright (c) 2026 Ángel, All rights reserved.
/// angel.rodriguez@udit.es

#include <HttpServer.hpp>
#include <iostream>
#include <NetworkSetup.hpp>
#include "SignalHandler.hpp"
#include "StaticFileServer.hpp"
#include "LuaServerApplication.hpp"

using namespace argb;

int main (int , const char * [])
{
    try
    {
        NetworkSetup network_setup;
        HttpServer   server;
        Port         port{ 80 };
        
        LuaServerApplication lua_server_app    ("../../examples/lua-server/main.lua");
        StaticFileServer     static_file_server("../../examples/static-website", "/");

        server.register_handler_factory (lua_server_app);
        server.register_handler_factory (static_file_server);

        std::cout << "Running HTTP server on port " << static_cast<uint16_t>(port) << "..." << std::endl;

        SignalHandler::handle (server);

        server.run (port);
    }
    catch (const NetworkException & exception)
    {
        std::cout << "Network exception: " << exception << std::endl;

        return 1;
    }

    return 0;
}
