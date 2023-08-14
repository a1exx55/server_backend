#ifndef HTTP_SESSION_HPP
#define HTTP_SESSION_HPP

//local
#include <logging/logger.hpp>
#include <config.hpp>
#include <jwt_utils/jwt_utils.hpp>
#include <request_handlers/request_handlers.hpp>

//internal
#include <iostream>
#include <fstream>
#include <memory>
#include <optional>
#include <filesystem>
#include <map>

///external
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>
#include <boost/json.hpp>
#include<boost/tokenizer.hpp>

namespace beast = boost::beast;  
namespace http = boost::beast::http;      
namespace net = boost::asio;            
namespace ssl = boost::asio::ssl;
namespace json = boost::json;       
using tcp = boost::asio::ip::tcp;

class http_session : public std::enable_shared_from_this<http_session>
{
    public:
        explicit http_session(tcp::socket&& socket, ssl::context& ssl_context);

        // Start the asynchronous http session
        void run();

    private:
        // Start ...
        void on_run();
        
        void on_handshake(beast::error_code error_code);

        void do_read_header();

        void on_read_header(beast::error_code error_code, std::size_t bytes_transferred);

        void do_read_body(const std::function<void(
            const http::request_parser<http::string_body>&, 
            http::response<http::string_body>&)>&);

        void on_read_body(const std::function<void(
                const http::request_parser<http::string_body>&, 
                http::response<http::string_body>&)>& request_handler, 
            beast::error_code error_code, 
            std::size_t bytes_transferred);

        void prepare_error_response(http::status response_status, const std::string& error_message);

        void do_write_response();

        void on_write_response(beast::error_code error_code, std::size_t bytes_transferred);

        void do_close();

        void on_shutdown(beast::error_code ec);

        bool validate_access_token();

        bool validate_refresh_token();

        beast::ssl_stream<beast::tcp_stream> _stream;
        beast::flat_buffer _buffer;
        // Wrap in std::optional to use parser several times by invoking .emplace() every request
        std::optional<http::request_parser<http::string_body>> _request_parser;
        http::response<http::string_body> _response;
        json::parser _body_json_parser;
        jwt_utils _jwt;
        // This map is used to determine what to do depends on the header target value
        const std::map<std::string, std::function<void()>> _target_to_handler_relations
        {
            {
                "/api/user/login", 
                [this]
                {
                    do_read_body(request_handlers::handle_login);
                }
            },
            {
                "/api/user/logout", 
                [this]
                {
                    if (!validate_refresh_token()) return;
                    request_handlers::handle_logout(*_request_parser, _response);
                }
            },
            {
                "/api/user/refresh", 
                [this]
                {
                    if (!validate_refresh_token()) return;
                    request_handlers::handle_refresh(*_request_parser, _response);
                }
            },
            {
                "/api/user/sessions_info", 
                [this]
                {
                    if (!validate_access_token()) return;
                    request_handlers::handle_sessions_info(*_request_parser, _response);
                }
            }
        };
}; 

#endif