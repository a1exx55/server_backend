#ifndef REQUEST_HANDLERS_HPP
#define REQUEST_HANDLERS_HPP

//local
#include <cookie_utils/cookie_utils.hpp>
#include <config.hpp> 
#include <database/database_pool.hpp>

//external
#include <boost/beast/http.hpp>

namespace http = boost::beast::http;

namespace request_handlers
{
    void handle_login(
        const http::request_parser<http::string_body>& request_parser, 
        http::response<http::string_body>& response);

    void handle_logout(
        const http::request_parser<http::string_body>& request_parser, 
        http::response<http::string_body>& response);

    void handle_refresh(
        const http::request_parser<http::string_body>& request_parser, 
        http::response<http::string_body>& response);

    void handle_sessions_info(
        const http::request_parser<http::string_body>& request_parser, 
        http::response<http::string_body>& response);

    void handle_upload_files(
        const http::request_parser<http::string_body>& request_parser, 
        http::response<http::string_body>& response);

    void prepare_error_response(
        http::response<http::string_body>& response, 
        const http::status response_status,
        const std::string_view& error_message);
};

#endif