#ifndef DATABASE_HPP
#define DATABASE_HPP

//local
#include <logging/logger.hpp>

//internal
#include <stdint.h>
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
        // Return the user id on finding, otherwise return emptry string
        // Return empty std::optional(std::nullopt) on fail
        std::optional<std::string> login(const std::string_view& username, const std::string_view& password);

        // Return folder path by the given folder id on folder existence otherwise return empty string
        // Return empty std::optional(std::nullopt) on fail
        std::optional<std::string> get_folder_path(const std::string& folder_id);

        std::optional<std::pair<std::string, std::string>> insert_file(
            const std::string& user_id,
            const std::string& folder_id, 
            const std::string_view& filename);
    private:
        // Try to reconnect to the database if connection has lost
        // Return true if the reconnection has succeed, otherwise return false 
        bool reconnect();

        std::optional<pqxx::connection> _conn;
        pqxx::result _result;
        // pqxx::nontransaction _nontrans;
};

#endif