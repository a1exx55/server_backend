#ifndef REQUEST_HANDLERS_HPP
#define REQUEST_HANDLERS_HPP

//local
#include <cookie_utils/cookie_utils.hpp>
#include <config.hpp> 
#include <database/database_connections_pool.hpp>
#include <network/request_and_response_params.hpp>
#include <network/uri_params.hpp>

//external
#include <boost/regex.hpp>

namespace http = boost::beast::http;

namespace request_handlers
{
    void prepare_error_response(
        response_params& response, 
        const http::status response_status,
        const std::string_view& error_message);
    
    void login(const request_params& request, response_params& response);

    void logout(const request_params& request, response_params& response);

    void refresh_tokens(const request_params& request, response_params& response);

    void get_sessions_info(const request_params& request, response_params& response);

    void close_session(const request_params& request, response_params& response);

    void close_all_sessions_except_current(const request_params& request, response_params& response);

    void change_password(const request_params& request, response_params& response);

    void get_folders_info(const request_params& request, response_params& response);

    void create_folder(const request_params& request, response_params& response);

    void delete_folders(const request_params& request, response_params& response);

    void rename_folder(const request_params& request, response_params& response);

    void get_files_info(const request_params& request, response_params& response);

    void process_downloaded_files(
        std::list<std::pair<size_t, std::string>>&& file_ids_and_paths,
        database_connection_wrapper&& db_conn);

    void delete_files(const request_params& request, response_params& response);

    void rename_file(const request_params& request, response_params& response);
};

#endif