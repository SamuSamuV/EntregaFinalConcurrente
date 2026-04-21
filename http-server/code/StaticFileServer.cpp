
/// @copyright Copyright (c) 2026 Ángel, All rights reserved.
/// angel.rodriguez@udit.es

#include <MimeType.hpp>
#include "StaticFileServer.hpp"
#include <stdexcept>

namespace argb
{

    StaticFileServer::StaticFileRequestHandler::StaticFileRequestHandler(std::string_view file_path_string)
        : file_path(file_path_string)
    {
        if (not std::filesystem::exists (file_path) || not std::filesystem::is_regular_file (file_path))
        {
            state = FILE_NOT_FOUND;
        }
        else
        {
            reader.open (file_path, std::ios::binary);
            
            state = reader.is_open () ? READING_FILE : INTERNAL_ERROR;
        }
    }

    bool StaticFileServer::StaticFileRequestHandler::process (const HttpRequest & request, HttpResponse & response)
    {
        switch (state)
        {
            case FILE_NOT_FOUND:
            {
                static constexpr std::string_view not_found_message = "File not found";
                send_plain_text_response (response, 404, not_found_message);
                state = FINISHED;
                return true;
            }

            case INTERNAL_ERROR:
            {
                static constexpr std::string_view internal_error_message = "Internal error";
                send_plain_text_response (response, 500, internal_error_message);    
                state = FINISHED;
                return true;
            }

            case READING_FILE:
            {
                bool finished = send_response (response);

                if (finished) state = FINISHED;
            
                return finished;
            }
        }

        return true;
    }

    bool StaticFileServer::StaticFileRequestHandler::send_response (HttpResponse & response)
    {
        // Aquí se podría implementar la lógica de lectura incremental del archivo y escritura en el cuerpo de la respuesta.
        // Por simplicidad, por el momento se lee todo el archivo de una vez.

        size_t content_length = std::filesystem::file_size (file_path);

        std::vector<char> file_content(content_length);

        reader.read (file_content.data (), content_length);

        if (not reader.good ())
        {
            state = INTERNAL_ERROR;
            return false;
        }
            
        HttpResponse::Serializer(response)
            .status     (200)
            .header     ("Content-Type",   MimeType::from (file_path.string ()).string)
            .header     ("Content-Length", std::to_string (content_length))
            .header     ("Connection",     "close")
            .end_header ()
            .body       (file_content);

        return true;
    }

    HttpRequestHandler::Ptr StaticFileServer::create_handler (HttpRequest::Method method, std::string_view request_path)
    {
        if (method == HttpRequest::Method::GET)
        {
            using namespace std::filesystem;

            if (request_path.empty () || request_path == "/")
            {
                request_path = "/index.html";
            }

            if (request_path.length () > requests_base_path.length () && request_path.starts_with (requests_base_path))
            {
                // Strip the leading '/' from the request path and resolve it against the local storage base path to get
                // the full file path:

                path full_path      = weakly_canonical (path{ local_storage_base_path } / path{ request_path.substr (1) });
                path canonical_base = weakly_canonical (path{ local_storage_base_path });

                // Reject any path that resolves outside the base directory (path traversal guard):

                auto base_end = std::mismatch
                (
                    canonical_base.begin (), canonical_base.end (),
                         full_path.begin (),      full_path.end ()
                )
                .first;

                if (base_end == canonical_base.end ())
                {
                    return { std::make_unique<StaticFileRequestHandler> (full_path.string ()) };
                }
            }
        }

        return {};
    }

}
