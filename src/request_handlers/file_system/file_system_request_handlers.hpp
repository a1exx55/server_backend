#ifndef FILE_SYSTEM_REQUEST_HANDLERS_HPP
#define FILE_SYSTEM_REQUEST_HANDLERS_HPP

// local
#include <request_handlers/error_response_preparing.hpp>
#include <config.hpp>
#include <database/database_connections_pool.hpp>
#include <database/file_system/file_system_database_connection.hpp>
#include <network/request_and_response_params.hpp>
#include <utils/http_utils/uri.hpp>
#include <parsing/file_types_conversion/file_types_conversion.hpp>
#include <parsing/csv_file_normalization/csv_file_normalization.hpp>
#include <parsing/file_preview/file_preview.hpp>

// external
#include <boost/algorithm/string.hpp>
#include <bit7z/bitarchivereader.hpp>

namespace http = boost::beast::http;      

namespace request_handlers
{
    class file_system
    {
        public:
            static void get_folders_info([[maybe_unused]] const request_params& request, response_params& response);

            static void get_file_rows_number(const request_params& request, response_params& response);

            static void get_file_raw_rows(const request_params& request, response_params& response);

            static void create_folder(const request_params& request, response_params& response);

            static void delete_folders(const request_params& request, response_params& response);

            static void rename_folder(const request_params& request, response_params& response);

            static void get_files_info(const request_params& request, response_params& response);

            static std::filesystem::path process_uploading_file(
                std::string_view uploading_file_name,
                std::list<std::tuple<size_t, std::filesystem::path, std::string>>& files_data,
                size_t user_id,
                size_t folder_id,
                database_connection_wrapper<file_system_database_connection>& db_conn,
                response_params& response);

            static void process_uploaded_file(
                [[maybe_unused]] const std::filesystem::path& uploading_file_path,
                std::list<std::tuple<size_t, std::filesystem::path, std::string>>& files_data,
                [[maybe_unused]] size_t user_id,
                [[maybe_unused]] size_t folder_id,
                database_connection_wrapper<file_system_database_connection>& db_conn,
                response_params& response);

            static void process_uploaded_files(
                std::list<std::tuple<size_t, std::filesystem::path, std::string>>&& files_data,
                size_t user_id,
                size_t folder_id,
                database_connection_wrapper<file_system_database_connection>&& db_conn);

            static void delete_files(const request_params& request, response_params& response);

            static void rename_file(const request_params& request, response_params& response);

        private:
            static void process_unzipping_archive(
                std::list<std::tuple<size_t, std::filesystem::path, std::string>>& files_data,
                std::list<std::tuple<size_t, std::filesystem::path, std::string>>::iterator file_data_it,
                size_t user_id,
                size_t folder_id,
                database_connection_wrapper<file_system_database_connection>& db_conn);

            static bool process_converting_file_to_csv(
                std::tuple<size_t, std::filesystem::path, std::string>& file_data,
                database_connection_wrapper<file_system_database_connection>& db_conn);

            static void process_normalizing_csv_file(
                std::tuple<size_t, std::filesystem::path, std::string>& file_data,
                size_t user_id,
                size_t folder_id,
                database_connection_wrapper<file_system_database_connection>& db_conn);
    };
}


#endif