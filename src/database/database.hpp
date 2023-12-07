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
        // Return the user id on finding, otherwise return 0(user ids start with 1)
        // Return empty std::optional(std::nullopt) on fail
        std::optional<size_t> login(const std::string_view& username, const std::string_view& password);

        // Return folder path by the given folder id on folder existence otherwise return empty string
        // Return empty std::optional(std::nullopt) on fail
        std::optional<std::string> get_folder_path(size_t folder_id);

        // Check if the file with given name exists in the specified folder
        // Return true on file existence, otherwise return false
        // Return empty std::optional(std::nullopt) on fail
        std::optional<bool> check_file_existence_by_name(size_t folder_id, const std::string_view& file_name);

        // Insert new file to the 'files' table
        // Return a pair of newly inserted file's id and path
        // Return empty std::optional(std::nullopt) on fail
        std::optional<std::pair<size_t, std::string>> insert_file(
            size_t user_id,
            size_t folder_id, 
            const std::string_view& folder_path,
            const std::string_view& file_name,
            const std::string_view& file_extension);

        // Delete the file from 'files' table by its id
        // Return empty std::optional(std::nullopt) on fail
        std::optional<bool> delete_file(size_t file_id);

        // Update 'files' table by setting file size, upload date with current time and changing status to 'uploaded'
        // Return empty std::optional(std::nullopt) on fail
        std::optional<bool> update_uploaded_file(size_t file_id, size_t file_size);
    private:
        // Try to reconnect to the database if connection has lost
        // Return true if the reconnection has succeed, otherwise return false 
        bool reconnect();

        std::optional<pqxx::connection> _conn;
        pqxx::result _result;
        // pqxx::nontransaction _nontrans;
};

#endif