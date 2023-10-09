#ifndef DATABASE_HPP
#define DATABASE_HPP

//internal
#include <stdint.h>
#include <iostream>
#include <optional>

//external
#include <pqxx/connection>
#include <pqxx/result>
#include <pqxx/transaction>
#include <pqxx/nontransaction>

class database
{
    public:
        // Connect to the database by given parameters
        // Can throw exception if it couldn't connect to the database
        database(
            const std::string& username, 
            const std::string& password, 
            const std::string& host, 
            const size_t port,
            const std::string& database_name);

        // Check if the user with given username and password exists in database
        // Return the user id on finding, otherwise return 0(users' ids start with 1)
        // Return empty std::optional(std::nullopt) on fail
        std::optional<size_t> login(const std::string_view& username, const std::string_view& password);
    private:
        // Try to reconnect to the database if connection has lost
        // Return true if the reconnection has succeed, otherwise return false 
        bool reconnect();

        std::optional<pqxx::connection> _conn;
        pqxx::result _result;
        // pqxx::nontransaction _nontrans;
};

#endif