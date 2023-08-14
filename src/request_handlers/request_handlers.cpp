#include <request_handlers/request_handlers.hpp>

void request_handlers::handle_login(
    const http::request_parser<http::string_body>& request_parser, 
    http::response<http::string_body>& response)
{
    response.erase(http::field::set_cookie);
    response.body() = request_parser.get()[http::field::accept_encoding];
    response.prepare_payload();
}

void request_handlers::handle_logout(
    const http::request_parser<http::string_body>& request_parser, 
    http::response<http::string_body>& response)
{
    
}

void request_handlers::handle_refresh(
    const http::request_parser<http::string_body>& request_parser, 
    http::response<http::string_body>& response)
{
    
}

void request_handlers::handle_sessions_info(
    const http::request_parser<http::string_body>& request_parser, 
    http::response<http::string_body>& response)
{
    
}

void request_handlers::handle_upload_files()
{
    
}