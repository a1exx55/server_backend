#ifndef USER_REQUEST_HANDLERS_HPP
#define USER_REQUEST_HANDLERS_HPP

// local
#include <request_handlers/error_response_preparing.hpp>
#include <config.hpp>
#include <database/database_connections_pool.hpp>
#include <database/user_database_connection/user_database_connection.hpp>
#include <network/request_and_response_params.hpp>
#include <utils/http_utils/uri.hpp>

// external
#include <boost/beast/http/status.hpp>
#include <boost/regex.hpp>

namespace http = boost::beast::http;      

namespace request_handlers
{
    class user
    {
        public:
            static void login(const request_params& request, response_params& response);

            static void logout(const request_params& request, response_params& response);

            static void refresh_tokens(const request_params& request, response_params& response);

            static void get_sessions_info(const request_params& request, response_params& response);

            static void close_session(const request_params& request, response_params& response);

            static void close_all_sessions_except_current(const request_params& request, response_params& response);

            static void change_password(const request_params& request, response_params& response);
    };
}

#endif