/// @copyright Copyright (c) 2026 Ángel, All rights reserved.
/// angel.rodriguez@udit.es

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

extern "C"
{
    typedef struct sqlite3      sqlite3;
    typedef struct sqlite3_stmt sqlite3_stmt;
}

namespace argb
{

    class Sqlite
    {
        using Database    = sqlite3;
        using Statement   = sqlite3_stmt;
        using byte_span   = std::span<const std::byte>;
        using string_view = std::string_view;

    public:

        class Row 
        {
            friend class Sqlite;

            Statement * statement;

        private:

            Row(Statement * statement) : statement(statement)
            {
            }

            void finalize () noexcept;

        public:

            Row(const Row & ) = delete;
            Row & operator = (const Row & ) = delete;

            Row(Row && other) noexcept
            {
                this->statement = other.statement;
                other.statement = nullptr;
            }

            Row & operator = (Row && other) noexcept
            {
                if (this != &other) 
                {
                    this->finalize ();
                    this->statement = other.statement;
                    other.statement = nullptr;
                }

                return *this;
            }

           ~Row() noexcept
            {
                finalize ();
            }

        public:

            template<typename TYPE>
            TYPE get (int column_index) const;

            bool advance ();

        private:

            void check_bounds_at (int column_index) const;

        };

    private:
        
        Database * database;

    public:

        Sqlite(std::filesystem::path database_path) : Sqlite(std::string_view(database_path.string ()))
        {
        }

        Sqlite(std::string_view database_path);

       ~Sqlite();

        Sqlite(const Sqlite & ) = delete;
        Sqlite & operator = (const Sqlite & ) = delete;

        Sqlite(Sqlite && other) noexcept
        {
            this->database = other.database;
            other.database = nullptr;
        }

        Sqlite & operator = (Sqlite && other) noexcept
        {
            if (this != &other) 
            {
                this->database = other.database;
                other.database = nullptr;
            }
            return *this;
        }

    public:

        using SqlValue = std::variant<bool, int32_t, int64_t, double, std::string>;

        void execute_all (string_view sql_code, std::span<const SqlValue> args);
        Row  query_all   (string_view sql_code, std::span<const SqlValue> args);

        template<typename... Args>
        void execute (string_view sql_code, Args... args)
        {
            Statement * statement = prepare (sql_code);

            if (statement == nullptr)
            {
                throw_runtime_error ("Error preparing SQL statement for execution: ");
            }

            Row row(statement);

            bind_all (statement, 1, args...);

            while (row.advance ())
            {
            }
        }

        template<typename... Args>
        Row query (string_view sql_code, Args... args) 
        {
            Statement * statement = prepare (sql_code);

            if (statement == nullptr)
            {
                throw_runtime_error ("Error preparing SQL statement for query: ");
            }

            Row row(statement);

            bind_all (statement, 1, args...);

            return row;
        }

    private:

        Sqlite::Statement * prepare (string_view sql_code);

        void bind (Statement * statement, int index, bool               value);
        void bind (Statement * statement, int index, int32_t            value);
        void bind (Statement * statement, int index, int64_t            value);
        void bind (Statement * statement, int index, double             value);
        void bind (Statement * statement, int index, byte_span          value);
        void bind (Statement * statement, int index, string_view        value);
        void bind (Statement * statement, int index, const std::string & value) { bind (statement, index, string_view(value)); }

        void bind_all (Statement * , int ) { }

        template<typename TYPE, typename... ARGUMENTS>
        void bind_all (Statement * statement, int index, TYPE value, ARGUMENTS... arguments) 
        {
            bind     (statement, index,     value       );
            bind_all (statement, index + 1, arguments...);
        }

    private:

        [[noreturn]] void throw_runtime_error (const char * message)
        {
            throw_runtime_error (message, database);
        }

        [[noreturn]] static void throw_runtime_error (const char * message, Database * database);

    };

}
