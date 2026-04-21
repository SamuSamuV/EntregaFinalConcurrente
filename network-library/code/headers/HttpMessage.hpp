
/// @copyright Copyright (c) 2026 Ángel, All rights reserved.
/// angel.rodriguez@udit.es

#pragma once

#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Socket.hpp"

namespace argb
{

    /** This class represents a generic HTTP message, which can be either a request or a response. It contains the common
      * components of an HTTP message, such as the header and body, and provides methods to access these components. This
      * class is designed to be a base class for more specific HTTP message types, such as HttpRequest and HttpResponse.
      */
    class HttpMessage
    {
    public:

        using Id = Socket::Handle;

    protected:

        using Buffer     = std::vector<char>;
        using Dictionary = std::map<std::string_view, std::string_view>;

    protected:

        Buffer     header;
        Dictionary headers;
        Buffer     body;

    public:

        HttpMessage ()
        {
        }

        HttpMessage(HttpMessage && other) noexcept
            : header (std::move (other.header ))
            , headers(std::move (other.headers))
            , body   (std::move (other.body   ))
        {
        }

        HttpMessage & operator = (HttpMessage && other) noexcept
        {
            if (this != &other)
            {
                header  = std::move (other.header );
                headers = std::move (other.headers);
                body    = std::move (other.body   );
            }

            return *this;
        }

        HttpMessage(const HttpMessage & ) = delete;
        HttpMessage & operator = (const HttpMessage & ) = delete;

    public:

        /** Retrieves the value of a header field by its name. The header field names are case-insensitive.
          * @param name The name of the header field to retrieve.
          * @return The value of the header field as a string view, or an empty string view if the header field is
          *     not found.
          */
        std::string_view get_header (std::string_view name) const
        {
            auto   item  = headers.find (name);
            return item != headers. end () ? item->second : std::string_view{};
        }

        /** Retrieves a span of characters representing the serialized header of the HTTP message. This span contains the
          * raw bytes of the header as it was received or serialized, including all header fields and metadata.
          * @return A span of characters representing the serialized header of the HTTP message.
          */
        std::span<const char> get_serialized_header () const
        {
            return header;
        }

        /** Retrieves a span of characters representing the body of the HTTP message. This span contains the raw bytes of the
          * body as it was received or serialized, without any interpretation or parsing.
          * @return A span of characters representing the body of the HTTP message.
          */
        std::span<const char> get_body () const
        {
            return body;
        }

    };

}
