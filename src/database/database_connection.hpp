#ifndef DATABASE_CONNECTION_HPP
#define DATABASE_CONNECTION_HPP

//local
#include <logging/logger.hpp>

//internal
#include <stdint.h>
#include <optional>

//external
#include <pqxx/connection>
#include <pqxx/result>
#include <pqxx/transaction>
#include <boost/json.hpp>
#include <magic_enum.hpp>

namespace json = boost::json;   

enum class file_status
{
    uploading,
    uploaded,
    unzipping,
    recoding,
    ready_for_parsing,
    parsing
};

class database_connection
{
    public:
        // Connect to the database by given parameters
        // Can throw exception if it couldn't connect to the database
        database_connection(
            std::string_view user_name, 
            std::string_view password, 
            std::string_view host, 
            size_t port,
            std::string_view database_name);

        // Check if the user with given user_name and password exists in database
        // Return the user id on finding, otherwise return 0(user ids start with 1)
        // Return empty std::optional on fail
        std::optional<size_t> login(std::string_view user_name, std::string_view password);

        // Insert refresh token to the 'refresh_tokens' table and insert session to the 'sessions' table with given data
        // Temporary session can be only one so before inserting we close the previous one
        // Return empty std::optional on fail
        std::optional<std::monostate> insert_session(
            size_t user_id, 
            std::string_view refresh_token, 
            std::string_view user_agent,
            std::string_view user_ip,
            bool is_temporary_session);

        std::optional<bool> close_current_session(std::string_view refresh_token, std::string_view user_ip);

        std::optional<bool> update_refresh_token(
            std::string_view old_refresh_token, 
            std::string_view new_refresh_token, 
            std::string_view user_ip);

        std::optional<json::object> get_sessions_info(size_t user_id, std::string_view refresh_token);

        std::optional<bool> close_own_session(size_t session_id, size_t user_id);

        std::optional<bool> close_all_sessions_except_current(size_t user_id, std::string_view refresh_token);

        std::optional<bool> validate_password(size_t user_id, std::string_view password);

        std::optional<bool> change_password(
            size_t user_id, 
            std::string_view new_password, 
            std::string_view refresh_token);

        std::optional<json::array> get_folders_info();

        std::optional<bool> check_folder_existence_by_name(std::string_view folder_name);

        std::optional<std::pair<json::object, std::string>> insert_folder(
            size_t user_id, 
            std::string_view user_name, 
            std::string_view folder_name);

        std::optional<std::pair<std::vector<size_t>, std::vector<std::string>>> delete_folders(
            const std::vector<size_t>& folder_ids); 

        std::optional<bool> rename_folder(size_t folder_id, std::string_view new_folder_name);

        std::optional<json::object> get_files_info(size_t folder_id);

        // Check if the folder with given id exists
        // Return true on folder existence, otherwise return false
        // Return empty std::optional on fail
        std::optional<bool> check_folder_existence_by_id(size_t folder_id);

        // Check if the file with given name exists in the specified folder
        // Return true on file existence, otherwise return false
        // Return empty std::optional on fail
        std::optional<bool> check_file_existence_by_name(size_t folder_id, std::string_view file_name);

        // Insert new file to the 'files' table
        // Return a pair of newly inserted file's id and path
        // Return empty std::optional on fail
        std::optional<std::pair<size_t, std::string>> insert_uploading_file(
            size_t user_id,
            size_t folder_id, 
            std::string_view file_name,
            std::string_view file_extension);

        // Delete the file from 'files' table by its id
        // Return empty std::optional on fail
        std::optional<std::monostate> delete_file(size_t file_id);

        // Update 'files' table by setting file size, upload date with current time and changing status to 'uploaded'
        // Return empty std::optional on fail
        std::optional<std::monostate> update_uploaded_file(size_t file_id, size_t file_size);

        std::optional<std::pair<size_t, std::string>> insert_unzipped_file(
            size_t user_id,
            size_t folder_id,
            std::string_view file_name,
            std::string_view file_extension,
            size_t file_size);

        std::optional<std::monostate> change_file_status(size_t file_id, file_status new_status);

        std::optional<std::pair<std::vector<size_t>, std::vector<std::string>>> delete_files(
            const std::vector<size_t>& file_ids); 

        std::optional<size_t> get_folder_id_by_file_id(size_t file_id);

        std::optional<bool> rename_file(size_t file_id, std::string_view new_file_name);
    private:
        // Try to reconnect to the database if connection has lost
        // Return true if the reconnection has succeed, otherwise return false 
        bool reconnect();

        bool close_all_sessions_except_current_impl(
            pqxx::work& transaction, 
            size_t user_id, 
            std::string_view refresh_token); 

        bool check_folder_existence_by_name_impl(
            pqxx::work& transaction, 
            std::string_view folder_name);

        bool check_file_existence_by_name_impl(
            pqxx::work& transaction, 
            size_t folder_id, 
            std::string_view file_name);

        std::optional<pqxx::connection> _conn;
        pqxx::result _result;
};

#endif