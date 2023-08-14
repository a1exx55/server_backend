#ifndef REQUEST_HANDLERS_HPP
#define REQUEST_HANDLERS_HPP

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

    void handle_upload_files();
}

#endif