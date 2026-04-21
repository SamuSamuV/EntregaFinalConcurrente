
/// @copyright Copyright (c) 2026 Ángel, All rights reserved.
/// angel.rodriguez@udit.es

#pragma once

#include <cassert>
#include <csignal>
#include <HttpServer.hpp>
#include <macros.hpp>

#if defined WINDOWS_OS
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

namespace argb
{

    /** Configures OS-level signal and control-event handlers to allow the HTTP server to shut down gracefully when a
      * termination request is received (e.g., Ctrl+C in the terminal, system shutdown, etc.).
      */
    class SignalHandler
    {

        inline static HttpServer * server = nullptr;

    public:

        SignalHandler() = delete;

        /** Sets up signal handlers for graceful shutdown of the HTTP server. Must be called before the server starts
          * running, and only once.
          * @param given_server A reference to the HttpServer instance to stop when a signal is received.
          */
        static void handle (HttpServer & given_server)
        {
            assert(server == nullptr);

            server = &given_server;

            std::signal (SIGINT,  signal_handler);
            std::signal (SIGTERM, signal_handler);

            // Ignore SIGPIPE to prevent the server from crashing when writing to a socket closed by the client:

            #if defined LINUX_OS || defined MAC_OS
                std::signal (SIGPIPE, SIG_IGN);
            #endif

            #if defined WINDOWS_OS
                SetConsoleCtrlHandler (console_control_handler, TRUE);
            #endif
        }

    private:

        #if defined WINDOWS_OS

            /** Handles console control events that do not trigger standard signal handlers, such as closing the terminal
              * window, logging off, or shutting down the system.
              * @param control_event The control event received.
              * @return TRUE if the event was handled and the process should not be terminated, or FALSE to pass the event
              *     to the next handler.
              */
            static BOOL WINAPI console_control_handler (DWORD control_event)
            {
                switch (control_event)
                {
                    case CTRL_CLOSE_EVENT:
                    case CTRL_LOGOFF_EVENT:
                    case CTRL_SHUTDOWN_EVENT:
                    {
                        if (server) server->stop ();
                        return TRUE;
                    }
                    default:
                        return FALSE;
                }
            }

        #endif

        /** Handles SIGINT and SIGTERM to allow graceful shutdown of the server.
          * @param signal The signal number received (not used, but required by the signature).
          */
        static void signal_handler ([[maybe_unused]] int signal)
        {
            if (server) server->stop ();
        }

    };

}
