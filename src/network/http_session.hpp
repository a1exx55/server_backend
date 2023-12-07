#ifndef HTTP_SESSION_HPP
#define HTTP_SESSION_HPP

//local
#include <logging/logger.hpp>
#include <config.hpp>
#include <jwt_utils/jwt_utils.hpp>
#include <request_handlers/request_handlers.hpp>

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
using request_handler_t = std::function<void(
                const http::request_parser<http::string_body>&, 
                http::response<http::string_body>&)>;
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

        void do_read_header();

        void on_read_header(beast::error_code error_code, std::size_t bytes_transferred);

        void do_read_body(const request_handler_t& request_handler);

        void on_read_body(
            const request_handler_t& request_handler, 
            beast::error_code error_code, 
            std::size_t bytes_transferred);

        void prepare_error_response(const http::status response_status, const std::string_view& error_message);

        void do_write_response(bool keep_alive = true);

        void on_write_response(bool keep_alive, beast::error_code error_code, std::size_t bytes_transferred);

        void do_close();

        // Return the uri excluding query or path parameters
        static std::string_view get_unparameterized_uri(const std::string_view& uri);

        // Match the path parameter to the given param_value_to_store, converting it to this variable type
        // Return true on successful conversion and assignment otherwise return false
        template <typename param_value_t>
        static bool get_path_parameter(
            const std::string_view& uri, 
            param_value_t& param_value_to_store);

        static json::object get_query_parameters(const std::string_view& uri, size_t expected_params_number = 1);

        // Match each query parameter by its name to the corresponding variable, converting it to this variable type
        // Return true on all successful conversions and assignments otherwise return false
        template <typename param_value_t, typename... params_t>
        static bool get_query_parameters(
            const std::string_view& uri, 
            const std::string_view& param_name, 
            param_value_t& param_value_to_store, 
            params_t&&... other_params);

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
            std::unique_ptr<database>&& db, 
            std::list<std::pair<size_t, std::string>>&& file_paths,
            beast::error_code error_code, std::size_t bytes_transferred);

        void handle_file_data(
            dynamic_buffer&& buffer, 
            std::string_view&& boundary,
            size_t folder_id,
            std::string&& folder_path, 
            std::ofstream&& file,
            std::unique_ptr<database>&& db, 
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
        
        // This map is used to determine what to do depends on the header target value
        inline static const std::unordered_map<
            std::pair<std::string_view, http::verb>, 
            std::tuple<bool, jwt_token_type, const request_handler_t>, 
            boost::hash<std::pair<std::string_view, http::verb>>> _requests_metadata
        {
            {
                {"/api/user/login", http::verb::post},
                {true, jwt_token_type::NO, request_handlers::handle_login}
            },
            {
                {"/api/user/logout", http::verb::post},
                {false, jwt_token_type::ACCESS_TOKEN, request_handlers::handle_logout}
            },
            {
                {"/api/file_system/files", http::verb::post},
                {true, jwt_token_type::ACCESS_TOKEN, [](
                    const http::request_parser<http::string_body>&, 
                    http::response<http::string_body>&){}}
            }
        };
}; 

#endif