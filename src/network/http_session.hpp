#ifndef HTTP_SESSION_HPP
#define HTTP_SESSION_HPP

//local
#include <logging/logger.hpp>
#include <config.hpp>
#include <jwt_utils/jwt_utils.hpp>
#include <request_handlers/request_handlers.hpp>
#include <network/request_and_response_params.hpp>
#include <network/uri_params.hpp>

//internal
#include <fstream>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <filesystem>

///external
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/json.hpp>
#include <boost/functional/hash.hpp>
#include <boost/algorithm/string.hpp>
#include <unicode/unistr.h>

namespace beast = boost::beast;  
namespace http = boost::beast::http;      
namespace asio = boost::asio; 
namespace ssl = boost::asio::ssl;
namespace json = boost::json;

using tcp = boost::asio::ip::tcp;
using request_handler_t = std::function<void(const request_params&, response_params&)>;
using dynamic_buffer = asio::dynamic_string_buffer<char, std::char_traits<char>, std::allocator<char>>;

class http_session : public std::enable_shared_from_this<http_session>
{
    public:
        explicit http_session(tcp::socket&& socket, ssl::context& ssl_context);

        // Start the asynchronous http session
        void run();

    private:
        void on_run();
        
        void on_handshake(beast::error_code error_code);

        void set_request_props();

        void do_read_header();

        void on_read_header(beast::error_code error_code, std::size_t bytes_transferred);

        void do_read_body(const request_handler_t& request_handler);

        void on_read_body(
            const request_handler_t& request_handler, 
            beast::error_code error_code, 
            std::size_t bytes_transferred);

        void prepare_error_response(const http::status response_status, std::string_view error_message);

        void do_write_response(bool keep_alive = true);

        void on_write_response(bool keep_alive, beast::error_code error_code, std::size_t bytes_transferred);

        void do_close();

        void parse_request_params();

        void parse_response_params();

        // Handle unexpected request attributes depending on whether the body is present or not
        bool validate_request_attributes(bool has_body);
      
        // Validate jwt token depending on its type
        bool validate_jwt_token(jwt_token_type token_type);

        void download_files();

        void handle_file_header(
            dynamic_buffer&& buffer, 
            std::string_view&& boundary,
            size_t folder_id,
            std::string&& folder_path, 
            std::ofstream&& file,
            database_connection_wrapper&& db, 
            std::list<std::pair<size_t, std::string>>&& file_paths,
            beast::error_code error_code, std::size_t bytes_transferred);

        void handle_file_data(
            dynamic_buffer&& buffer, 
            std::string_view&& boundary,
            size_t folder_id,
            std::string&& folder_path, 
            std::ofstream&& file,
            database_connection_wrapper&& db, 
            std::list<std::pair<size_t, std::string>>&& file_paths,
            beast::error_code error_code, std::size_t bytes_transferred);

        beast::ssl_stream<beast::tcp_stream> _stream;
        // Main buffer to use in read/write operations
        beast::flat_buffer _buffer;
        // String buffer for downloading files
        // Can't use the main one due to using asio read operations instead of beast
        std::string _files_string_buffer;
        // Wrap in std::optional to use parser several times by invoking .emplace() every request
        std::optional<http::request_parser<http::string_body>> _request_parser;
        http::response<http::string_body> _response;
        // Encapsulation of request and response params to pass them to request handlers 
        // to avoid direct access to _request_parser and _response
        request_params _request_params;
        response_params _response_params;
        
        // This map is used to determine what to do depends on the header target value
        inline static const std::unordered_map<
            std::tuple<std::string_view, http::verb, uri_params::type>, 
            std::tuple<bool, jwt_token_type, const request_handler_t>, 
            boost::hash<std::tuple<std::string_view, http::verb, uri_params::type>>> _requests_metadata
        {
            {
                {"/api/user/login", http::verb::post, uri_params::type::NO},
                {true, jwt_token_type::NO, request_handlers::login}
            },
            {
                {"/api/user/logout", http::verb::post, uri_params::type::NO},
                {false, jwt_token_type::REFRESH_TOKEN, request_handlers::logout}
            },
            {
                {"/api/user/tokens", http::verb::put, uri_params::type::NO},
                {false, jwt_token_type::REFRESH_TOKEN, request_handlers::refresh_tokens}
            },
            {
                {"/api/user/sessions", http::verb::get, uri_params::type::NO},
                {false, jwt_token_type::REFRESH_TOKEN, request_handlers::get_sessions_info}
            },
            {
                {"/api/user/sessions", http::verb::delete_, uri_params::type::PATH},
                {false, jwt_token_type::ACCESS_TOKEN, request_handlers::close_session}
            },
            {
                {"/api/user/sessions", http::verb::delete_, uri_params::type::NO},
                {false, jwt_token_type::REFRESH_TOKEN, request_handlers::close_all_sessions_except_current}
            },
            {
                {"/api/user/password", http::verb::put, uri_params::type::NO},
                {true, jwt_token_type::REFRESH_TOKEN, request_handlers::change_password}
            },
            {
                {"/api/file_system/folders", http::verb::post, uri_params::type::NO},
                {true, jwt_token_type::ACCESS_TOKEN, request_handlers::create_folder}
            },
            {
                {"/api/file_system/files", http::verb::get, uri_params::type::QUERY},
                {false, jwt_token_type::ACCESS_TOKEN, request_handlers::get_files_info}
            },
            {
                {"/api/file_system/folders", http::verb::get, uri_params::type::NO},
                {false, jwt_token_type::ACCESS_TOKEN, request_handlers::get_folders_info}
            },
            {
                {"/api/file_system/files", http::verb::post, uri_params::type::QUERY},
                {true, jwt_token_type::ACCESS_TOKEN, [](const request_params&, response_params&){}}
            }
        };
}; 

#endif