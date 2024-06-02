#ifndef FILE_SYSTEM_DATABASE_CONNECTION_HPP
#define FILE_SYSTEM_DATABASE_CONNECTION_HPP

//local
#include <database/database_connection.hpp>
#include <logging/logger.hpp>

//internal
#include <optional>
#include <format>

//external
#include <boost/json.hpp>
#include <magic_enum.hpp>

namespace json = boost::json;   

enum class file_status
{
    uploading,
    uploaded,
    unzipping,
    converting,
    normalizing,
    ready_for_parsing,
    parsing
};

class file_system_database_connection : public database_connection
{
    public:
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

        std::optional<std::string> get_file_name(size_t file_id);

        std::optional<std::string> get_file_path(size_t file_id);

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
        std::optional<std::tuple<size_t, std::filesystem::path, std::string>> insert_uploading_file(
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

        std::optional<std::tuple<size_t, std::filesystem::path, std::string>> insert_processed_file(
            size_t user_id,
            size_t folder_id,
            std::string_view file_name,
            std::string_view file_extension,
            size_t file_size,
            file_status file_status);

        std::optional<std::monostate> change_file_status(size_t file_id, file_status new_status);

        // Update 'files' table by changing extension, size and updating path with the new extension
        std::optional<std::monostate> update_processed_file(
            size_t file_id, 
            std::string_view new_file_extension, 
            std::string_view new_file_path, 
            size_t new_file_size);

        std::optional<std::pair<std::vector<size_t>, std::vector<std::string>>> delete_files(
            const std::vector<size_t>& file_ids); 

        std::optional<size_t> get_folder_id_by_file_id(size_t file_id);

        std::optional<bool> rename_file(size_t file_id, std::string_view new_file_name);
        
    private:
        bool check_folder_existence_by_name_impl(
            pqxx::work& transaction, 
            std::string_view folder_name);

        bool check_file_existence_by_name_impl(
            pqxx::work& transaction, 
            size_t folder_id, 
            std::string_view file_name);
};

#endif