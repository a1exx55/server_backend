#include <network/http_session.hpp>

http_session::http_session(tcp::socket&& socket, ssl::context& ssl_context)
    :_stream(std::move(socket), ssl_context) 
{
    _response.keep_alive(true);
    _response.version(11);
    _response.set(http::field::server, "OCSearch");
    _response.set(
        http::field::host, 
        config::SERVER_IP_ADDRESS + ":" + std::to_string(config::SERVER_PORT));
}

void http_session::run()
{
    // We need to be executing within a strand to perform async operations
    // on the I/O objects as we used make_strand in listener to accept connection
    asio::dispatch(
        _stream.get_executor(),
        beast::bind_front_handler(
            &http_session::on_run,
            shared_from_this()));
}

void http_session::on_run()
{
    // Set the timeout.
    beast::get_lowest_layer(_stream).expires_after(std::chrono::seconds(30));
    
    // Perform the SSL handshake
    _stream.async_handshake(
        ssl::stream_base::server,
        beast::bind_front_handler(
            &http_session::on_handshake,
            shared_from_this()));
}

void http_session::on_handshake(beast::error_code error_code)
{
    // The errors mean that client closed the connection 
    if(error_code == http::error::end_of_stream || error_code == asio::ssl::error::stream_truncated)
    {
        return do_close();
    }

    // The error means that the timer on the logical operation(write/read) is expired
    if (error_code == beast::error::timeout)
    {
        return do_close();
    }

    if (error_code)
    {
        LOG_ERROR << error_code.message();
        return;
    }

    do_read_header();
}

void http_session::do_read_header()
{
    // Make the request parser empty before reading new request,
    // otherwise the operation behavior is undefined
    _request_parser.emplace();

    // Set unlimited body to prevent "body limit exceeded" error
    // and handle the real limit in validate_request_attributes 
    _request_parser->body_limit(boost::none);

    // Fix the error "bad method" that happens if read only the header when there is a body in request
    _buffer.clear();

    // Set the timeout for next operation
    beast::get_lowest_layer(_stream).expires_after(std::chrono::seconds(30));
    
    // Read a request header
    http::async_read_header(_stream, _buffer, *_request_parser,
        beast::bind_front_handler(
            &http_session::on_read_header,
            shared_from_this()));
}

void http_session::on_read_header(beast::error_code error_code, std::size_t bytes_transferred)
{
    // Suppress compiler warnings about unused variable bytes_transferred  
    boost::ignore_unused(bytes_transferred);

    // The errors mean that client closed the connection 
    if(error_code == http::error::end_of_stream || error_code == asio::ssl::error::stream_truncated)
    {
        return do_close();
    }

    // The error means that the timer on the logical operation(write/read) is expired
    if (error_code == beast::error::timeout)
    {
        return do_close();
    }

    if (error_code)
    {
        LOG_ERROR << error_code.message();
        return do_close();
    }   

    // Reset the timeout
    beast::get_lowest_layer(_stream).expires_never();

    // Determine the handler to invoke by request target 
    // or construct error response if didn't found corresponding target
    if (auto request_metadata = _requests_metadata.find(
            {
                get_unparameterized_uri(_request_parser->get().target()), 
                _request_parser->get().method()
            }); 
            request_metadata != _requests_metadata.end())
    {
        // Check if the request attributes meets the requirements depending on the expected body presence
        if (!validate_request_attributes(std::get<0>(request_metadata->second)))
        {
            return do_write_response(false);
        }

        // Validate jwt token(access or refresh) with the presence
        if (!validate_jwt_token(std::get<1>(request_metadata->second)))
        {
            return do_write_response(false);
        }

        // Read the body with the presence and invoke request handler
        if (std::get<0>(request_metadata->second))
        {
            // Process downloading files separately because it is not synchronous as other handlers 
            if (request_metadata->first == 
                std::pair<std::string_view, http::verb>{"/api/file_system/files", http::verb::post})
            {
                return download_files();
            }
            do_read_body(std::get<2>(request_metadata->second));
        }
        else
        {
            std::get<2>(request_metadata->second)(*_request_parser, _response);
            do_write_response();
        }
    }
    else
    {
        prepare_error_response(
            http::status::not_found, 
            "URI " + std::string(_request_parser->get().target()) + " with method " + 
                std::string(http::to_string(_request_parser->get().method())) + " was not found");
        do_write_response(false);
    }
}

void http_session::do_read_body(const request_handler_t& request_handler)
{   
    // Set the timeout.
    beast::get_lowest_layer(_stream).expires_after(std::chrono::seconds(30));
    
    // Read a request header
    http::async_read(_stream, _buffer, *_request_parser,
        beast::bind_front_handler(
            &http_session::on_read_body,
            shared_from_this(), 
            request_handler));
}

void http_session::on_read_body(
    const request_handler_t& request_handler, 
    beast::error_code error_code, 
    std::size_t bytes_transferred)
{
    // Suppress compiler warnings about unused variable bytes_transferred  
    boost::ignore_unused(bytes_transferred);

    // The error means that client closed the connection
    if(error_code == http::error::end_of_stream || error_code == asio::ssl::error::stream_truncated)
    {
        return do_close();
    }

    // The error means that the timer on the logical operation(write/read) is expired
    if (error_code == beast::error::timeout)
    {
        return do_close();
    }

    if (error_code)
    {
        LOG_ERROR << error_code.message();
        return do_close();
    }   

    // Reset the timeout as we may do long request handling 
    beast::get_lowest_layer(_stream).expires_never();

    request_handler(*_request_parser, _response);
    do_write_response();
}

void http_session::do_write_response(bool keep_alive)
{
    // Set the timeout for next operation
    beast::get_lowest_layer(_stream).expires_after(std::chrono::seconds(30));

    // Write the response
    http::async_write(
        _stream,
        _response,
        beast::bind_front_handler(
            &http_session::on_write_response,
            shared_from_this(), 
            keep_alive));
}

void http_session::on_write_response(bool keep_alive, beast::error_code error_code, std::size_t bytes_transferred)
{
    // Suppress compiler warnings about unused variable bytes_transferred  
    boost::ignore_unused(bytes_transferred);

    // The error means that the timer on the logical operation(write/read) is expired
    if (error_code == beast::error::timeout)
    {
        return do_close();
    }

    if (error_code)
    {
        LOG_ERROR << error_code.message();
        return do_close();
    }

    // Determine whether we have to read the next request or close the connection
    // depending on whether the response was regular or error
    if (keep_alive)
    {
        do_read_header();
    }
    else
    {
        do_close();
    }
}

void http_session::do_close()
{
    // Set the timeout.
    beast::get_lowest_layer(_stream).expires_after(std::chrono::seconds(30));

    // Perform the SSL shutdown
    _stream.async_shutdown([self = shared_from_this()](beast::error_code){});
}

void http_session::prepare_error_response(const http::status response_status, const std::string_view& error_message)
{
    // Clear unnecessary field
    _response.erase(http::field::set_cookie);
    
    _response.result(response_status);
    _response.body().assign(R"({"error":")").append(error_message).append(R"("})");
    _response.prepare_payload();
}

std::string_view http_session::get_unparameterized_uri(const std::string_view& uri)
{
    // Start with position 5 because the uri has to start with /api/... and no need to capture these slashes
    size_t delimiter_position = 5;

    // Skip the next slash as it is the part of the uri
    if ((delimiter_position = uri.find('/', 5)) == std::string::npos)
    {
        return {};
    }

    // If we find the 4th slash in the uri then the path parameters start
    if ((delimiter_position = uri.find('/', delimiter_position + 1)) != std::string::npos)
    {
        return uri.substr(0, delimiter_position);
    }

    // If we find the question mark in the uri then the query parameters start
    if ((delimiter_position = uri.find('?', 0)) != std::string::npos)
    {
        return uri.substr(0, delimiter_position);
    }

    // The uri was unparameterized from the start
    return uri;
}

template <typename param_value_t>
bool http_session::get_path_parameter(
    const std::string_view& uri, 
    param_value_t& param_value_to_store)
{
    // Start with position 5 because the uri has to start with /api/... and no need to capture these slashes
    size_t slash_position = 5;

    // Skip the next slash as it is the part of the uri
    slash_position = uri.find('/', 5);

    // If we didn't find the 4th slash in the uri then there is no path parameter
    if ((slash_position = uri.find('/', slash_position + 1)) == std::string::npos)
    {
        return false;
    }

    // If we found the next slash then there are too many slashes for our uri
    if (uri.find('/', slash_position + 1) != std::string::npos)
    {
        return false;
    }

    // Path parameter is a string
    if constexpr (std::is_same_v<param_value_t, std::string>)
    {
        param_value_to_store = uri.substr(slash_position + 1);
    }
    // Path parameter is a number
    else if constexpr (std::is_integral_v<param_value_t>)
    {
        // Unsigned integers are got from string to uint64_t
        if constexpr (std::is_unsigned_v<param_value_t>)
        {
            try
            {
                param_value_to_store = std::stoull(std::string{uri.substr(slash_position + 1)});
            }
            // Path parameter string value representation is not a number
            catch (const std::exception&)
            {
                return false;
            }
        }
        // Signed integers are got from string to int64_t
        else if constexpr (std::is_signed_v<param_value_t>)
        {
            try
            {
                param_value_to_store = std::stoll(std::string{uri.substr(slash_position + 1)});
            }
            // Path parameter string value representation is not a number
            catch (const std::exception&)
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }

    return true;
}

json::object http_session::get_query_parameters(const std::string_view& uri, size_t expected_params_number)
{
    // Start with position 5 because the uri has to start with /api/... and no need to capture these slashes
    size_t delimiter_position = 5;

    // Skip the next slash as it is the part of the uri
    delimiter_position = uri.find('/', 5);

    // If we found the 4th slash in the uri then there is path parameter instead of query ones
    if (uri.find('/', delimiter_position + 1) != std::string::npos)
    {
        return {};
    }

    // Query parameters must start with question mark
    if ((delimiter_position = uri.find('?', delimiter_position + 1)) == std::string::npos)
    {
        return {};
    }

    size_t start_position = delimiter_position + 1, end_position;
    json::object query_parameters;
    
    try
    {
        // Reserve the expected number of query parameters to avoid reallocating memory 
        query_parameters.reserve(expected_params_number);

        do
        {
            delimiter_position = uri.find('=', start_position);

            // No equals sign so it is invalid format
            if (delimiter_position == std::string::npos)
            {
                return {};
            }

            end_position = uri.find('&', delimiter_position + 1);

            // If the key ends with [] then it is the array parameter
            // otherwise it is just the regular string parameter
            if (uri.substr(delimiter_position - 2, 2) == "[]")
            {
                // If there are array elements with the current key then just emplace the current value back
                // otherwise emplace the array with the current value to the current key
                if (query_parameters.contains(uri.substr(start_position, delimiter_position - start_position - 2)))
                {
                    query_parameters[uri.substr(start_position, delimiter_position - start_position - 2)]
                        .as_array().emplace_back(uri.substr(
                            delimiter_position + 1, 
                            end_position - delimiter_position - 1));
                }
                else
                {
                    query_parameters.emplace(
                        uri.substr(start_position, delimiter_position - start_position - 2), 
                        json::array{uri.substr(delimiter_position + 1, end_position - delimiter_position - 1)});
                }
            }
            else
            {
                query_parameters.emplace(
                    uri.substr(start_position, delimiter_position - start_position),
                    uri.substr(delimiter_position + 1, end_position - delimiter_position - 1));
            }

            start_position = end_position + 1;
        } 
        while (end_position != std::string::npos);
    }
    catch (const std::exception&)
    {
        return {};
    }

    return query_parameters;
}

template <typename param_value_t, typename... params_t>
bool http_session::get_query_parameters(
    const std::string_view& uri, 
    const std::string_view& param_name, 
    param_value_t& param_value_to_store, 
    params_t&&... other_params) 
{
    size_t start_position{0}, equal_sign_position, end_position;

    // Query parameter is a string
    if constexpr (std::is_same_v<param_value_t, std::string>)
    {
        start_position = uri.find(param_name);

        // No query parameter found
        if (start_position == std::string::npos)
        {
            return false;
        }

        equal_sign_position = start_position + param_name.size();

        // No equal sign so it is invalid format
        if (equal_sign_position >= uri.size() || uri[equal_sign_position] != '=')
        {
            return false;
        }

        end_position = uri.find('&', equal_sign_position + 1);

        param_value_to_store = uri.substr(equal_sign_position + 1, end_position - equal_sign_position - 1);
    }
    // Query parameter is a number
    else if constexpr (std::is_integral_v<param_value_t>)
    {
        start_position = uri.find(param_name);

        // No query parameter found
        if (start_position == std::string::npos)
        {
            return false;
        }

        equal_sign_position = start_position + param_name.size();

        // No equal sign so it is invalid format
        if (equal_sign_position >= uri.size() || uri[equal_sign_position] != '=')
        {
            return false;
        }

        end_position = uri.find('&', equal_sign_position + 1);

        // Unsigned integers are got from string to uint64_t
        if constexpr (std::is_unsigned_v<param_value_t>)
        {
            try
            {
                param_value_to_store = std::stoull(
                        std::string{uri.substr(equal_sign_position + 1, end_position - equal_sign_position - 1)},
                        &start_position);

                // Query parameter string value representation contains non digits after valid number
                if (start_position != 
                    uri.substr(equal_sign_position + 1, end_position - equal_sign_position - 1).size())
                {
                    return false;
                }
            }
            // Query parameter string value representation is not a number
            catch (const std::exception&)
            {
                return false;
            }
        }
        // Signed integers are got from string to int64_t
        else if constexpr (std::is_signed_v<param_value_t>)
        {
            try
            {
                param_value_to_store = std::stoll(
                    std::string{uri.substr(equal_sign_position + 1, end_position - equal_sign_position - 1)});

                // Query parameter string value representation contains non digits after valid number
                if (start_position != 
                    uri.substr(equal_sign_position + 1, end_position - equal_sign_position - 1).size())
                {
                    return false;
                }
            }
            // Query parameter string value representation is not a number
            catch (const std::exception&)
            {
                return false;
            }
        }
    }
    // Query parameter is an array(process only std::vector<>)
    else if constexpr (std::is_same_v<
        param_value_t, std::vector<typename param_value_t::value_type, typename param_value_t::allocator_type>>)
    {
        param_value_to_store.clear();

        do
        {
            start_position = uri.find(param_name, start_position);

            // No query parameter found
            if (start_position == std::string::npos)
            {
                // There have to be some array query parameters with given name
                if (param_value_to_store.empty())
                {
                    return false;
                }
                else
                {
                    break;
                }
            }

            equal_sign_position = start_position + param_name.size() + 2;

            // No equal sign so it is invalid format
            if (equal_sign_position >= uri.size() || uri[equal_sign_position] != '=')
            {
                return false;
            }

            // Array parameter name has to be followed by []
            if (uri.substr(equal_sign_position - 2, 2) != "[]")
            {
                return false;
            }

            end_position = start_position = uri.find('&', equal_sign_position + 1);

            // Array elements type is a string
            if constexpr (std::is_same_v<typename param_value_t::value_type, std::string>)
            {
                param_value_to_store.emplace_back(
                    uri.substr(equal_sign_position + 1, end_position - equal_sign_position - 1));
            }
            //Array elements type is a number
            else if constexpr (std::is_integral_v<typename param_value_t::value_type>)
            {
                // Unsigned integers are got from string to uint64_t
                if constexpr (std::is_unsigned_v<typename param_value_t::value_type>)
                {
                    try
                    {
                        param_value_to_store.emplace_back(std::stoull(std::string{uri.substr(
                            equal_sign_position + 1, end_position - equal_sign_position - 1)}));
                    }
                    // Query parameter string value representation is not a number
                    catch (const std::exception&)
                    {
                        return false;
                    }
                }
                // Signed integers are got from string to int64_t
                else if constexpr (std::is_signed_v<typename param_value_t::value_type>)
                {
                    try
                    {
                        param_value_to_store.emplace_back(std::stoll(std::string{uri.substr(
                            equal_sign_position + 1, end_position - equal_sign_position - 1)}));
                    }
                    // Query parameter string value representation is not a number
                    catch (const std::exception&)
                    {
                        return false;
                    }
                }
            }
        } while (end_position != std::string::npos);  
    }
    else
    {
        return false;
    }

    // Call the same function if there are query parameters left to process
    if constexpr (sizeof...(other_params) > 0) 
    {
        if (!get_query_parameters(uri, std::forward<params_t>(other_params)...))
        {
            return false;
        }
    }

    return true;
}

bool http_session::validate_request_attributes(bool has_body)
{
    // Request with body
    if (has_body)
    {
        // We expect body octets but there are no
        if (_request_parser->is_done())
        {
            prepare_error_response(
                http::status::unprocessable_entity, 
                "Body octets were expected");
            return false;
        }
        
        // Content-Length absence causes difficult debugging of the body errors
        if (!_request_parser->content_length())
        {
            prepare_error_response(
                http::status::length_required, 
                "No Content-Length field");
            return false;
        }

        // // Forbid requests with body size more than 1 MB(there is exception for uploading files)
        // if (*_request_parser->content_length() > 1024 * 1024)
        // {
        //     prepare_error_response(
        //         http::status::payload_too_large, 
        //         "Too large body size");
        //     return false;
        // }

        return true;
    }
    // Request with no body
    else
    {
        // We don't expect body octets but they are there
        if (!_request_parser->is_done())
        {
            prepare_error_response(
                http::status::unprocessable_entity, 
                "No body octets were expected");
            return false;
        }

        // Content-Length absence causes difficult debugging of the body errors 
        if (!_request_parser->content_length())
        {
            prepare_error_response(
                http::status::length_required, 
                "No Content-Length field");
            return false;
        }

        // Request without body is expected Content-Length to be zero
        if (*_request_parser->content_length() != 0)
        {
            prepare_error_response(
                http::status::length_required, 
                "Content-Length field was expected to be zero");
            return false;
        }

        return true;
    }
}

bool http_session::validate_jwt_token(jwt_token_type token_type)
{
    switch (token_type)
    {
        case (jwt_token_type::NO):
        {
            return true;
        }

        case (jwt_token_type::ACCESS_TOKEN):
        {
            // Get access token from the Authorization field in the http header in the format of "Bearer <token>"
            // so the strings are less than the length 8 can't be at all(don't try to predict access token length) 
            if (_request_parser->get()[http::field::authorization].size() < 8 || 
                !jwt_utils::is_token_valid(_request_parser->get()[http::field::authorization].substr(7)))
            {
                prepare_error_response(http::status::unauthorized, "Invalid access token");
                return false;
            }

            return true;
        }

        case (jwt_token_type::REFRESH_TOKEN):
        {
            // Get the refresh token from the Cookie field in the http header 
            // by parsing cookies and getting the value of the key "refreshToken"
            if (!jwt_utils::is_token_valid(
                std::string{cookie_utils::get_cookie_value(
                    _request_parser->get()[http::field::cookie], 
                    "refreshToken")}))
            {
                prepare_error_response(http::status::unauthorized, "Invalid refresh token");
                return false;
            }

            return true;
        } 

        default:
        {
            return false;
        }    
    }
}

void http_session::download_files()
{
    size_t folder_id;

    if (!get_query_parameters(_request_parser->get().target(), "folderId", folder_id))
    {
        prepare_error_response(
            http::status::unprocessable_entity, 
            "Invalid folder id as query parameter");
        return do_write_response(false);
    }

    auto db = database_pool::get();

    // No available connections
    if (!db)
    {
        prepare_error_response(
            http::status::internal_server_error, 
            "No available database connections");
        return do_write_response(false);
    }

    auto folder_path = db->get_folder_path(folder_id);
    
    // An error occured with database connection
    if (!folder_path)
    {
        database_pool::release(std::move(db));

        prepare_error_response(
            http::status::internal_server_error, 
            "Internal server error occured");
        return do_write_response(false);
    }

    // Folder with folder_id doesn't exist
    if (*folder_path == "")
    {
        database_pool::release(std::move(db));

        prepare_error_response(
            http::status::unprocessable_entity, 
            "Invalid folder id as query parameter");
        return do_write_response(false);
    }
    
    // Transfer data from the main buffer to our string buffer as some part of body 
    // could be previously obtained while reading the header
    _files_string_buffer.assign(asio::buffer_cast<const char*>(_buffer.data()), _buffer.size());

    // Use special wrapper for string as buffer for downloading files
    // Main buffer of type beast::flat_buffer is incompatible here(works weird)
    // Set the maximum size of the downloading packet to 10 MB
    dynamic_buffer buffer{_files_string_buffer, 1024 * 1024 * 10};

    std::string_view content_type = _request_parser->get()[http::field::content_type];
    size_t boundary_position = content_type.find("boundary=");

    // Boundary was not found in the content type
    if (boundary_position == std::string::npos)
    {
        database_pool::release(std::move(db));

        prepare_error_response(
            http::status::unprocessable_entity, 
            "Invalid Content-Type");
        return do_write_response(false);
    }

    // Determine the boundary for multipart/form-data content type
    std::string_view boundary = content_type.substr(boundary_position + 9);

    // Declare file variable to write obtained data to the corresponding files 
    std::ofstream file;

    // Store the ids and paths for all downloaded files to process them afterward(e.g. to recode or unzip for archives)
    std::list<std::pair<size_t, std::string>> file_ids_and_paths;

    // Set the timeout
    beast::get_lowest_layer(_stream).expires_after(std::chrono::seconds(30));

    // Read the boundary before the first header of file 
    asio::async_read_until(_stream, buffer, boundary, 
        beast::bind_front_handler(
            [this, self = shared_from_this()](
                dynamic_buffer&& buffer, 
                std::string_view&& boundary,
                size_t folder_id,
                std::string&& folder_path, 
                std::ofstream&& file, 
                std::unique_ptr<database>&& db,
                std::list<std::pair<size_t, std::string>>&& file_ids_and_paths,
                beast::error_code error_code, std::size_t bytes_transferred)
            {
                if (error_code)
                {
                    database_pool::release(std::move(db));

                    return do_close();
                }

                // Consume read bytes as it is just the boundary
                buffer.consume(bytes_transferred);

                // Set the timeout
                beast::get_lowest_layer(_stream).expires_after(std::chrono::seconds(30));

                // Read the first file header obtaining bytes until the empty string 
                // that represents the delimeter between file header and data itself
                asio::async_read_until(_stream, buffer, "\r\n\r\n", 
                    beast::bind_front_handler( 
                        &http_session::handle_file_header, 
                        shared_from_this(), 
                        std::move(buffer), 
                        std::move(boundary), 
                        folder_id, 
                        std::move(folder_path), 
                        std::move(file),
                        std::move(db),
                        std::move(file_ids_and_paths)));
            }, 
            std::move(buffer), 
            std::move(boundary), 
            folder_id, 
            std::move(*folder_path), 
            std::move(file),
            std::move(db),
            std::move(file_ids_and_paths)));
    
}

void http_session::handle_file_header(
    dynamic_buffer&& buffer, 
    std::string_view&& boundary,
    size_t folder_id,
    std::string&& folder_path, 
    std::ofstream&& file,
    std::unique_ptr<database>&& db, 
    std::list<std::pair<size_t, std::string>>&& file_ids_and_paths,
    beast::error_code error_code, std::size_t bytes_transferred)
{
    if (error_code)
    {
        database_pool::release(std::move(db));

        // If there are downloaded files then start separately processing them and close the connection
        if (!file_ids_and_paths.empty())
        {
            std::thread{request_handlers::process_downloaded_files, std::move(file_ids_and_paths)}.detach();
        }

        return do_close();
    }

    // Reset the timeout
    beast::get_lowest_layer(_stream).expires_never();

    // Construct the string representation of buffer
    std::string_view string_buffer{asio::buffer_cast<const char*>(buffer.data()), bytes_transferred};

    // Position of the filename field in the file header
    size_t file_name_position = string_buffer.find("filename");

    // filename field is absent
    if (file_name_position == std::string::npos)
    {
        database_pool::release(std::move(db));

        // If there are downloaded files then start separately processing them and return response
        if (!file_ids_and_paths.empty())
        {
            std::thread{request_handlers::process_downloaded_files, std::move(file_ids_and_paths)}.detach();
        }

        prepare_error_response(
            http::status::unprocessable_entity, 
            "No filename field found");
        return do_write_response(false);
    }

    // Shift to the filename itself(e.g. filename="file.txt")
    file_name_position += 10;

    size_t user_id;

    // This error means that access token was somehow generated incorrectly
    if (!jwt_utils::get_token_claim(
        std::string{_request_parser->get()[http::field::authorization].substr(7)}, 
        "userId", 
        user_id))
    {
        database_pool::release(std::move(db));

        // If there are downloaded files then start separately processing them and return response
        if (!file_ids_and_paths.empty())
        {
            std::thread{request_handlers::process_downloaded_files, std::move(file_ids_and_paths)}.detach();
        }

        prepare_error_response(
            http::status::unauthorized, 
            "Invalid access token");
        return do_write_response(false);
    }

    std::string file_name = std::string{string_buffer.substr(
        file_name_position, string_buffer.find('"', file_name_position) - file_name_position)};
    file_name_position = file_name.find_last_of('.');
    std::string file_extension = file_name.substr(file_name_position + 1);
    file_name = file_name.substr(0, file_name_position);

    static const std::unordered_set<std::string> allowed_file_extensions
    {
        "csv", "txt", "zip", "sql", "xlsx", "xls"
    };

    // Invalid file extension
    if (!allowed_file_extensions.contains(file_extension))
    {
        database_pool::release(std::move(db));

        // If there are downloaded files then start separately processing them and return response
        if (!file_ids_and_paths.empty())
        {
            std::thread{request_handlers::process_downloaded_files, std::move(file_ids_and_paths)}.detach();
        }

        prepare_error_response(
            http::status::unprocessable_entity, 
            "Filename contains invalid extension ." + std::string{file_extension});
        return do_write_response(false);
    }

    // Check if the string contains only space symbols
    // In this case consider it as the empty string and assign it with "(1)" like it is the copy of empty string 
    // because it is forbidden to have empty file name
    if (std::all_of(file_name.begin(), file_name.end(), [](unsigned char c) { return std::isspace(c); }))
    {
        file_name = "(1)";
    }

    // Transform the string to UnicodeString to work with any unicode symbols
    icu::UnicodeString file_name_unicode{file_name.c_str()};

    // Trim the string to the allowed length limit
    if (file_name_unicode.length() > 64)
    {
        file_name_unicode.retainBetween(0, 64);
    }

    file_name.clear();
    file_name_unicode.toUTF8String(file_name);

    std::optional<bool> does_file_exist;
    size_t opening_bracket, copy_number, non_digit_pos;

    // Check if the file with specified name exists in database
    // If so we create names with copies by adding number after it: test.txt -> test(1).txt -> test(2).txt
    // until the modified name is available in database
    while (true)    
    {
        does_file_exist = db->check_file_existence_by_name(folder_id, file_name);

        if (!does_file_exist)
        {
            database_pool::release(std::move(db));

            // If there are downloaded files then start separately processing them and return response
            if (!file_ids_and_paths.empty())
            {
                std::thread{request_handlers::process_downloaded_files, std::move(file_ids_and_paths)}.detach();
            }
            
            prepare_error_response(
                http::status::internal_server_error, 
                "Internal server error occured");
            return do_write_response(false);
        }

        // We found the available file name so stop creating copies
        if (!*does_file_exist)
        {
            break;
        }

        // If couldn't find opening and closing brackets then there is no copy yet
        // so just append '(1)' to the end of the file name
        if (file_name.back() == ')' && 
            (opening_bracket = file_name.rfind('(', file_name.size() - 2)) != std::string::npos)
        {
            try
            {
                // Try to consider data between opening and closing brackets as copy number
                copy_number = std::stoull(
                    file_name.substr(opening_bracket + 1, file_name.size() - opening_bracket - 2), &non_digit_pos);

                // Copy number string contains non digits after valid number
                if (non_digit_pos != 
                    file_name.substr(opening_bracket + 1, file_name.size() - opening_bracket - 2).size())
                {
                    throw std::exception{};
                }

                // If copy number is valid then just increment it by one in the file name
                file_name.replace(
                    opening_bracket + 1, file_name.size() - opening_bracket - 2, std::to_string(copy_number + 1));
            }
            // If we got here then the copy number is not valid(e.g. the brackets don't represent the copy number
            // but just are the part of the file name data - 'test(modified).txt') 
            // so just append the '(1)' to the end of the file name
            catch (const std::exception&)
            {
                file_name.append("(1)");
            }
        }
        else
        {
            file_name.append("(1)");
        }       
    }

    std::optional<std::pair<size_t, std::string>> file_id_and_path = db->insert_file(
        user_id,
        folder_id,
        folder_path,
        file_name,
        file_extension);

    // An error occured with database connection
    if (!file_id_and_path)
    {
        database_pool::release(std::move(db));

        // If there are downloaded files then start separately processing them and return response
        if (!file_ids_and_paths.empty())
        {
            std::thread{request_handlers::process_downloaded_files, std::move(file_ids_and_paths)}.detach();
        }
        
        prepare_error_response(
            http::status::internal_server_error, 
            "Internal server error occured");
        return do_write_response(false);
    }

    // Create and open the file to write the obtaining data
    file.open(file_id_and_path->second, std::ios::binary);

    // Store id and path of the current file to process it after the download(e.g. to recode or unzip for archives)
    file_ids_and_paths.emplace_back(*file_id_and_path);

    // Consume the file header bytes 
    buffer.consume(bytes_transferred);

    // Set the timeout
    beast::get_lowest_layer(_stream).expires_after(std::chrono::seconds(30));

    // Read the file data obtaining bytes until the boundary that represents the end of file
    asio::async_read_until(_stream, buffer, boundary, 
        beast::bind_front_handler( 
            &http_session::handle_file_data, 
            shared_from_this(), 
            std::move(buffer), 
            std::move(boundary), 
            folder_id, 
            std::move(folder_path), 
            std::move(file),
            std::move(db),
            std::move(file_ids_and_paths)));
}

void http_session::handle_file_data(
    dynamic_buffer&& buffer, 
    std::string_view&& boundary,
    size_t folder_id,
    std::string&& folder_path, 
    std::ofstream&& file,
    std::unique_ptr<database>&& db, 
    std::list<std::pair<size_t, std::string>>&& file_ids_and_paths,
    beast::error_code error_code, std::size_t bytes_transferred)
{
    // File can't be read at once as it is too big(>10Mb)
    // Process obtained packet and go on reading
    if (error_code == asio::error::not_found)
    {
        // Write obtained packet to the file
        // Don't touch last symbols with boundary length as we could stop in the middle of boundary
        // so we would write to the file the part of boundary 
        file.write(asio::buffer_cast<const char*>(buffer.data()), buffer.size() - boundary.size());

        // Consume written bytes
        buffer.consume(buffer.size() - boundary.size());

        // Set the timeout
        beast::get_lowest_layer(_stream).expires_after(std::chrono::seconds(30));

        // Read the next data until either we find a boundary or read the packet of maximum size again 
        asio::async_read_until(_stream, buffer, boundary, 
            beast::bind_front_handler( 
                &http_session::handle_file_data, 
                shared_from_this(), 
                std::move(buffer), 
                std::move(boundary), 
                folder_id, 
                std::move(folder_path), 
                std::move(file),
                std::move(db),
                std::move(file_ids_and_paths)));
        return;
    }

    // Define actions to clean up all the data about the file
    auto process_file_cleanup = 
    [&]
    {
        // Remove the file from the file system
        try
        {
            std::filesystem::remove(file_ids_and_paths.back().second);
        }
        catch (const std::exception& ex)
        {
            LOG_ERROR << ex.what();
        }

        // Delete the file from the database
        db->delete_file(file_ids_and_paths.back().first);

        // Remove the file from the list of downloaded files 
        file_ids_and_paths.pop_back();

        // If there are downloaded files then start separately processing them and return response
        if (!file_ids_and_paths.empty())
        {
            std::thread{request_handlers::process_downloaded_files, std::move(file_ids_and_paths)}.detach();
        }
    };

    // Unexpected error occured so clean up everything about not downloaded file
    if (error_code)
    {
        file.close();

        process_file_cleanup();

        database_pool::release(std::move(db));

        return do_close();
    }

    // Reset the timeout
    beast::get_lowest_layer(_stream).expires_never();

   // Write obtained bytes to the file excluding CRLF after the file data and -- followed by boundary
   // -- is the part of the boundary, used only in body, so we have to consider this -- length because
   // boudary variable doesn't contain it
    file.write(asio::buffer_cast<const char*>(buffer.data()), bytes_transferred - boundary.size() - 4);

    // Close the file as its downloading is over
    file.close();

    // Consume obtained bytes
    buffer.consume(bytes_transferred); 

    size_t file_size;

    // Determine the file size of just downloaded file
    try
    {
        file_size = std::filesystem::file_size(file_ids_and_paths.back().second);
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();

        process_file_cleanup();
   
        database_pool::release(std::move(db));

        prepare_error_response(
            http::status::internal_server_error, 
            "Internal server error occured");
        return do_write_response(false);
    }

    // Update the data about just downloaded file
    if (!db->update_uploaded_file(file_ids_and_paths.back().first, file_size))
    {
        process_file_cleanup();
   
        database_pool::release(std::move(db));

        prepare_error_response(
            http::status::internal_server_error, 
            "Internal server error occured");
        return do_write_response(false);
    }

    // If there is "--" after the boundary then there are no more files and request body is over
    // So start processing downloaded files and send response
    if (std::string_view{asio::buffer_cast<const char*>(buffer.data()), buffer.size()} == "--\r\n")
    {
        database_pool::release(std::move(db));

        std::thread{request_handlers::process_downloaded_files, std::move(file_ids_and_paths)}.detach();

        _response.erase(http::field::set_cookie);
        _response.result(http::status::ok);
        _response.body().clear(); 
        _response.prepare_payload();

        return do_write_response(true);
    }
    
    // Set the timeout
    beast::get_lowest_layer(_stream).expires_after(std::chrono::seconds(30));
    
    // Read the next file header
    asio::async_read_until(_stream, buffer, "\r\n\r\n", 
        beast::bind_front_handler( 
            &http_session::handle_file_header, 
            shared_from_this(), 
            std::move(buffer), 
            std::move(boundary), 
            folder_id, 
            std::move(folder_path), 
            std::move(file),
            std::move(db),
            std::move(file_ids_and_paths)));
}