#ifndef DATABASE_CONNECTION_HPP
#define DATABASE_CONNECTION_HPP

//local
#include <logging/logger.hpp>

//internal
#include <optional>
#include <format>

//external
#include <pqxx/connection>
#include <pqxx/result>
#include <pqxx/transaction>

class database_connection
{
    public:
        database_connection(){};

        database_connection(database_connection&& other_database_connection) 
            : _conn{std::move(other_database_connection._conn)}, _result{std::move(other_database_connection._result)}
        {
            other_database_connection._conn.reset();
        }

        operator bool() 
        {
            return _conn.operator bool();
        }
        
        // Connect to the database by given parameters
        // Can throw exception if it couldn't connect to the database
        database_connection(
            std::string_view user_name, 
            std::string_view password, 
            std::string_view host, 
            size_t port,
            std::string_view database_name)
            : _conn
            {   
                std::format(
                    "user={} password={} host={} port={} dbname={}",
                    user_name,
                    password,
                    host,
                    port,
                    database_name)
            }{}
            
    protected:
        // Try to reconnect to the database if connection has lost
        // Return true if the reconnection has succeed, otherwise return false 
        bool reconnect()
        {
            try
            {
                _conn.emplace(_conn->connection_string());
            }
            catch (const std::exception&)
            {
                return false;
            }
        
            return true;
        }
        
        std::optional<pqxx::connection> _conn;
        pqxx::result _result;
};

#endif