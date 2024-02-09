#ifndef FILE_SYSTEM_REQUEST_HANDLERS_HPP
#define FILE_SYSTEM_REQUEST_HANDLERS_HPP

// local
#include <request_handlers/error_response_preparing.hpp>
#include <config.hpp>
#include <database/database_connections_pool.hpp>
#include <network/request_and_response_params.hpp>
#include <network/uri_params.hpp>
#include <parsing/file_types_conversion/file_types_conversion.hpp>
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

            static void process_uploaded_files(
                std::list<std::tuple<size_t, std::filesystem::path, std::string>>&& file_ids_and_paths,
                size_t user_id,
                size_t folder_id,
                database_connection_wrapper&& db_conn);

            static void delete_files(const request_params& request, response_params& response);

            static void rename_file(const request_params& request, response_params& response);

        private:
            static void unzip_archives(
                std::list<std::tuple<size_t, std::filesystem::path, std::string>>& file_ids_and_paths,
                size_t user_id,
                size_t folder_id,
                database_connection_wrapper& db_conn);

            static void convert_files_to_csv(
                std::list<std::tuple<size_t, std::filesystem::path, std::string>>& file_ids_and_paths,
                database_connection_wrapper& db_conn);
    };
}


#endif