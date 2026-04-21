
/// @copyright Copyright (c) 2026 Ángel, All rights reserved.
/// angel.rodriguez@udit.es

#pragma once

#include "HttpMessage.hpp"

namespace argb
{

    /** This class represents an HTTP response message, which is a specific type of HTTP message that contains information
      * about the server's response to the client's request. It includes components such as the HTTP status code, headers,
      * and body. The HttpResponse class provides methods to access these components and is designed to be used in conjunction
      * with the HttpMessage base class to handle the processing of HTTP responses.
      */
    class HttpResponse : public HttpMessage
    {
    public:

        class Parser
        {
        };

        /** The Serializer class is responsible for serializing the HttpResponse object into a format suitable for transmission
          * over the network. It takes the components of the HttpResponse, such as the status code, headers, and body, and
          * constructs a properly formatted HTTP response message. The Serializer class is designed to be used internally by
          * the HttpResponse class to handle the serialization logic.
          */
        class Serializer
        {
            HttpResponse * response;

            bool status_serialized;
            bool header_serialized;

        public:

            Serializer(HttpResponse & target_response);

            Serializer & status     (int status);
            Serializer & header     (std::string_view name, std::string_view value);
            Serializer & end_header ();
            Serializer & body       (std::span<const char> content);

        private:

            void serialize_protocol_version ();
            void serialize_status (int status);
            void serialize_header (std::string_view name, std::string_view value);

            std::string_view get_status_message () const;
        };

    private:

        int status;

    public:

        HttpResponse() : status{}
        {
        }

        HttpResponse(int given_status) : status(given_status)
        {
        }

    public:

        /** Retrieves the HTTP status code of the response. The status code is a three-digit integer that indicates the
          * result of the HTTP request. Common status codes include 200 (OK), 404 (Not Found) and 500 (Internal Server Error).
          * @return The HTTP status code of the response as an integer.
          */
        int get_status () const
        {
            return status;
        }

    };

}
