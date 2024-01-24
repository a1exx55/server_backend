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

        void prepare_error_response(http::status response_status, std::string_view error_message);

        void do_write_response(bool keep_alive);

        void on_write_response(bool keep_alive, beast::error_code error_code, std::size_t bytes_transferred);

        void do_close();

        void parse_request_params();

        void parse_response_params();

        // Handle unexpected request attributes depending on the body present 
        // and if the request is for downloading files
        bool validate_request_attributes(bool has_body, bool is_downloading_files_request);
      
        // Validate jwt token depending on its type
        bool validate_jwt_token(jwt_token_type token_type);

        void upload_files();

        void process_uploading_file_header(
            dynamic_buffer&& buffer, 
            std::string_view&& boundary,
            size_t user_id,
            size_t folder_id,
            std::ofstream&& file,
            database_connection_wrapper&& db, 
            std::list<std::pair<size_t, std::string>>&& file_paths,
            beast::error_code error_code, std::size_t bytes_transferred);

        void process_uploading_file_data(
            dynamic_buffer&& buffer, 
            std::string_view&& boundary,
            size_t user_id,
            size_t folder_id,
            std::ofstream&& file,
            database_connection_wrapper&& db_conn, 
            std::list<std::pair<size_t, std::string>>&& file_paths,
            beast::error_code error_code, std::size_t bytes_transferred);

        beast::ssl_stream<beast::tcp_stream> _stream;
        // Main buffer to use in read/write operations
        beast::flat_buffer _buffer;
        // String buffer for downloading files
        // Can't use the main one due to using asio read operations instead of beast
        std::string _files_string_buffer;
        // Wrap parser in std::optional to use it several times as it can't be manually cleared 
        std::optional<http::request_parser<http::string_body>> _request_parser;
        http::response<http::string_body> _response;
        // Encapsulate request and response params to pass them to request handlers 
        // to avoid direct access to _request_parser and _response
        request_params _request_params;
        response_params _response_params;
        
        // This map is used to determine what request handler to invoke depending on the request params
        static const std::unordered_map<
            std::tuple<std::string_view, http::verb, uri_params::type>, 
            std::tuple<bool, jwt_token_type, const request_handler_t>, 
            boost::hash<std::tuple<std::string_view, http::verb, uri_params::type>>> _requests_metadata;
}; 

#endif