#ifndef HTTP_SESSION_HPP
#define HTTP_SESSION_HPP

//local
#include <logging/logger.hpp>
#include <config.hpp>
#include <utils/jwt_utils/jwt_utils.hpp>
#include <request_handlers/request_handlers.hpp>
#include <network/request_and_response_params.hpp>
#include <utils/http_utils/uri.hpp>
#include <utils/http_utils/http_endpoints_storage.hpp>
#include <multipart_form_data/downloader.hpp>

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

        void do_read_uploading_files();

        void on_read_uploading_files(
            beast::error_code error_code, 
            std::vector<std::filesystem::path>&& file_paths,
            std::list<std::tuple<size_t, std::filesystem::path, std::string>>&& files_data, 
            size_t user_id, 
            size_t folder_id, 
            database_connection_wrapper<file_system_database_connection>&& db_conn,
            [[maybe_unused]] response_params& response);

        void prepare_error_response(http::status response_status, std::string_view error_message);

        void do_write_response(bool keep_alive);

        void on_write_response(bool keep_alive, beast::error_code error_code, std::size_t bytes_transferred);

        void do_close();

        void parse_request_params();

        void parse_response_params();

        // Handle unexpected request attributes depending on the body presence 
        // and if the request is for uploading files
        bool validate_request_attributes(bool has_body, bool is_downloading_files_request);
      
        // Validate jwt token depending on its type
        bool validate_jwt_token(jwt_token_type token_type);

        beast::ssl_stream<beast::tcp_stream> _stream;
        // Main buffer to use in read/write operations
        beast::flat_buffer _buffer;
        // Wrap parser in std::optional to use it several times as it can't be manually cleared 
        std::optional<http::request_parser<http::string_body>> _request_parser;
        http::response<http::string_body> _response;
        // Wrapper over asio operations to perform files downloading via multipart/form-data protocol
        multipart_form_data::downloader<beast::ssl_stream<beast::tcp_stream>, beast::flat_buffer> _form_data;
        // Encapsulate request and response params to pass them to request handlers 
        // to avoid direct access to _request_parser and _response
        request_params _request_params;
        response_params _response_params;
        
        // Storage of http endpoints data to perform fast search of endpoints even with path parameters 
        static const http_utils::http_endpoints_storage
            <std::tuple<bool, jwt_token_type, request_handler_t>> _endpoints;
}; 

#endif