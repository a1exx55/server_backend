#ifndef HTTP_SESSION_HPP
#define HTTP_SESSION_HPP

//local
#include <logging/logger.hpp>
#include <config.hpp>

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
    beast::ssl_stream<beast::tcp_stream> _stream;
    beast::flat_buffer _buffer;
    std::optional<http::request_parser<http::empty_body>> _header_request_parser;
    std::optional<http::request_parser<http::string_body>> _string_request_parser;
    std::optional<http::request_parser<http::buffer_body>> _file_request_parser;
    http::response<http::empty_body> _header_response;
    http::response<http::string_body> _string_response;
    json::parser _body_json_parser;

    // static const json::value _target_tokens;
    const std::map<std::string, std::function<void()>> _target_to_function_relations =
    {
        {"/api/user/login", std::bind(&http_session::handle_login, this)},
        {"/api/user/logout", std::bind(&http_session::handle_logout, this)},
        {"/api/user/refresh/web", std::bind(&http_session::handle_refresh_web, this)},
        {"/api/user/refresh/desktop", std::bind(&http_session::handle_refresh_desktop, this)}
    };

public:
    // Take ownership of the socket
    explicit http_session(
        tcp::socket&& socket,
        ssl::context& ssl_context)
        :_stream(std::move(socket), ssl_context) 
    {
        _string_response.keep_alive(true);
        _string_response.version(11);
        _string_response.set(http::field::server, "OCSearch");
        _string_response.set(http::field::host, config::SERVER_IP_ADDRESS + ":" + std::to_string(config::SERVER_PORT));
    }

    // Start the asynchronous http session
    void run();

private:
    // Start ...
    void on_run();
    
    void on_handshake(beast::error_code error_code);

    void do_read_header();

    void on_read_header(beast::error_code error_code, std::size_t bytes_transferred);

    void handle_header();

    void send_error_response(http::status response_status, const std::string& error_message);

    void send_response(http::message_generator&& msg);

    void on_write(beast::error_code error_code, std::size_t bytes_transferred);

    void do_close();

    void on_shutdown(beast::error_code ec);

    void handle_login();
    void handle_logout();
    void handle_refresh_web();
    void handle_refresh_desktop();
    void handle_sessions_info();
}; 

#endif