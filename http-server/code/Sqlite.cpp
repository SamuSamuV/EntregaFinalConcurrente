
/// @copyright Copyright (c) 2026 Ángel, All rights reserved.
/// angel.rodriguez@udit.es

#include <cassert>
#include "Sqlite.hpp"
#include <sqlite3.h>
#include <stdexcept>
#include <string>

namespace argb
{

    void Sqlite::Row::finalize () noexcept
    {
        if (statement)
        {
            sqlite3_finalize (statement);
            statement = nullptr;
        }
    }

    template<>
    bool Sqlite::Row::get<bool> (int column_index) const
    {
        check_bounds_at (column_index);
        return sqlite3_column_int (statement, column_index) != 0;
    }

    template<>
    int32_t Sqlite::Row::get<int32_t> (int column_index) const
    {
        check_bounds_at (column_index);
        return static_cast<int32_t>(sqlite3_column_int (statement, column_index));
    }

    template<>
    int64_t Sqlite::Row::get<int64_t> (int column_index) const
    {
        check_bounds_at (column_index);
        return sqlite3_column_int64 (statement, column_index);
    }

    template<>
    double Sqlite::Row::get<double> (int column_index) const
    {
        check_bounds_at (column_index);
        return sqlite3_column_double (statement, column_index);
    }   

    template<>
    std::string Sqlite::Row::get<std::string> (int column_index) const
    {
        check_bounds_at (column_index);
        const char * text = reinterpret_cast<const char*>(sqlite3_column_text (statement, column_index));
        return text ? std::string(text) : std::string();
    }

    template<>
    std::string_view Sqlite::Row::get<std::string_view> (int column_index) const
    {
        check_bounds_at (column_index);
        const char * text = reinterpret_cast<const char*>(sqlite3_column_text (statement, column_index));
        return text ? std::string_view(text) : std::string_view();
    }

    template<>
    std::span<const std::byte> Sqlite::Row::get<std::span<const std::byte>> (int column_index) const
    {
        check_bounds_at(column_index);

        const void * blob = sqlite3_column_blob (statement, column_index);

        if (blob == nullptr)
        {
            return {};
        }

        int size = sqlite3_column_bytes (statement, column_index);

        if (size <= 0)
        {
            return {};
        }

        return std::span<const std::byte>(reinterpret_cast<const std::byte*>(blob), size);
    }

    bool Sqlite::Row::advance ()
    {
        if (statement == nullptr)
        {
            return false;
        }

        int result = sqlite3_step (statement);

        if (result != SQLITE_ROW && result != SQLITE_DONE)
        {
            throw_runtime_error ("Error executing SQL statement to next row: ", sqlite3_db_handle (statement));
        }

        return result == SQLITE_ROW;
    }

    void Sqlite::Row::check_bounds_at (int column_index) const
    {
        if (statement == nullptr)
        {
            throw std::runtime_error("Trying to access a column on an invalid or finalized row.");
        }

        int column_count = sqlite3_column_count (statement);

        if (column_index < 0 || column_index >= column_count)
        {
            throw std::out_of_range("Column index " + std::to_string (column_index) + " is out of range. Valid range is [0, " + std::to_string (column_count - 1) + "].");
        }
    }

    Sqlite::Sqlite(std::string_view database_path) : database(nullptr)
    {
        if (sqlite3_open (database_path.data (), &database) != SQLITE_OK) 
        {
            throw_runtime_error ("Failed to open database: ");
        }

        // Set a busy timeout of 5000 milliseconds (5 seconds) to handle database locks gracefully:

        sqlite3_busy_timeout (database, 5000); 
    }

    Sqlite::~Sqlite()
    {
        if (database)
        {
            sqlite3_close (database);
            database = nullptr;
        }
    }

    Sqlite::Statement * Sqlite::prepare (string_view sql_code)
    {
        Statement * statement = nullptr;

        if (sqlite3_prepare_v2 (database, sql_code.data (), static_cast<int>(sql_code.size ()), &statement, nullptr) != SQLITE_OK)
        {
            return nullptr;
        }

        return statement;
    }

    void Sqlite::bind (Statement * statement, int index, bool value)
    {
        if (sqlite3_bind_int (statement, index, value ? 1 : 0) != SQLITE_OK)
        {
            throw_runtime_error ("Error binding a boolean value: ");
        }
    }

    void Sqlite::bind (Statement * statement, int index, int32_t value)
    {
        if (sqlite3_bind_int (statement, index, value) != SQLITE_OK)
        {
            throw_runtime_error ("Error binding an int32 value: ");
        }
    }

    void Sqlite::bind (Statement * statement, int index, int64_t value)
    {
        if (sqlite3_bind_int64 (statement, index, value) != SQLITE_OK)
        {
            throw_runtime_error ("Error binding an int64 value: ");
        }
    }

    void Sqlite::bind (Statement * statement, int index, double value)
    {
        if (sqlite3_bind_double (statement, index, value) != SQLITE_OK)
        {
            throw_runtime_error ("Error binding a double value: ");
        }
    }

    void Sqlite::bind (Statement * statement, int index, byte_span value)
    {
        if (sqlite3_bind_blob (statement, index, value.data (), static_cast<int>(value.size ()), SQLITE_TRANSIENT) != SQLITE_OK)
        {
            throw_runtime_error ("Error binding a blob value: ");
        }
    }

    void Sqlite::bind (Statement * statement, int index, string_view value)
    {
        if (sqlite3_bind_text (statement, index, value.data (), static_cast<int>(value.size ()), SQLITE_TRANSIENT) != SQLITE_OK)
        {
            throw_runtime_error ("Error binding a string value: ");
        }
    }

    void Sqlite::execute_all (string_view sql_code, std::span<const SqlValue> args)
    {
        Statement * statement = prepare (sql_code);

        if (statement == nullptr)
        {
            throw_runtime_error ("Error preparing SQL statement for execution: ");
        }

        Row row(statement);

        int index = 1;

        for (const SqlValue & arg : args)
        {
            std::visit ([&] (const auto & value) { bind (statement, index++, value); }, arg);
        }

        while (row.advance ())
        {
        }
    }

    Sqlite::Row Sqlite::query_all (string_view sql_code, std::span<const SqlValue> args)
    {
        Statement * statement = prepare (sql_code);

        if (statement == nullptr)
        {
            throw_runtime_error ("Error preparing SQL statement for query: ");
        }

        Row row(statement);

        int index = 1;

        for (const SqlValue & arg : args)
        {
            std::visit ([&] (const auto & value) { bind (statement, index++, value); }, arg);
        }

        return row;
    }

    void Sqlite::throw_runtime_error (const char * message, Database * database)
    {
        assert(message != nullptr && "The message parameter must not be null.");

        throw std::runtime_error
        (
            database
                ? message + std::string(sqlite3_errmsg (database))
                : message
        );
    }

}   
