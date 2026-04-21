/// @copyright Copyright (c) 2026 Ángel, All rights reserved.
/// angel.rodriguez@udit.es

#pragma once

#include <HttpRequest.hpp>
#include <LuaState.hpp>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace argb
{

    template<typename TYPE>
    struct LuaTypeAdapter
    {
        using LuaType = TYPE;

        static decltype(auto) adapt_from_lua (LuaType value)
        {
            return value;
        }

        static TYPE adapt_for_lua (TYPE value)
        {
            return value;
        }
    };

    template<>
    struct LuaTypeAdapter<std::string>
    {
        using LuaType = std::string;

        static std::string && adapt_from_lua (std::string & value)
        {
            return std::move (value);
        }

        static std::string adapt_for_lua (std::string value)
        {
            return value;
        }
    };

    template<>
    struct LuaTypeAdapter<const std::string &>
    {
        using LuaType = std::string;

        static const std::string & adapt_from_lua (const std::string & value)
        {
            return value;
        }

        static std::string adapt_for_lua (const std::string & value)
        {
            return value;
        }
    };

    template<>
    struct LuaTypeAdapter<std::string_view>
    {
        using LuaType = std::string;

        static std::string_view adapt_from_lua (const std::string & value)
        {
            return value;
        }

        static std::string adapt_for_lua (std::string_view value)
        {
            return std::string(value);
        }
    };

    template<>
    struct LuaTypeAdapter<const std::string_view>
    {
        using LuaType = std::string;

        static std::string_view adapt_from_lua (const std::string & value)
        {
            return value;
        }

        static std::string adapt_for_lua (std::string_view value)
        {
            return std::string(value);
        }
    };

    template<>
    struct LuaTypeAdapter<const std::string_view &>
    {
        using LuaType = std::string;

        static std::string_view adapt_from_lua (const std::string & value)
        {
            return value;
        }

        static std::string adapt_for_lua (std::string_view value)
        {
            return std::string(value);
        }
    };

    template<>
    struct LuaTypeAdapter<std::span<const char>>
    {
        using LuaType = std::string;

        static std::string adapt_for_lua (std::span<const char> value)
        {
            return std::string(value.data (), value.size ());
        }
    };

    template<>
    struct LuaTypeAdapter<HttpRequest::Method>
    {
        using LuaType = std::string;

        static HttpRequest::Method adapt_from_lua (const std::string & value)
        {
            return HttpRequest::Parser::method_from_string (value);
        }

        static std::string adapt_for_lua (HttpRequest::Method value)
        {
            return std::string(HttpRequest::Serializer::method_to_string (value));
        }
    };

}
