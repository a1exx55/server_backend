#include <network/http_session.hpp>

http_session::http_session(tcp::socket&& socket, ssl::context& ssl_context)
    :
    _stream(std::move(socket), ssl_context),
    _request_params
    {
        .body = _request_parser->get().body()
    },
    _response_params
    {
        .body = _response.body()
    } 
{
    _response.keep_alive(true);
    _response.version(11);
    _response.set(http::field::access_control_allow_credentials, "true");
    _response.set(http::field::access_control_allow_origin, config::DOMAIN_NAME);
    _response.set(http::field::access_control_allow_methods, "OPTIONS, GET, POST, PUT, PATCH, DELETE");
    _response.set(http::field::access_control_allow_headers, "Content-Type, Authorization");
}

const std::unordered_map<
    std::tuple<std::string_view, http::verb, uri_params::type>, 
    std::tuple<bool, jwt_token_type, const request_handler_t>, 
    boost::hash<std::tuple<std::string_view, http::verb, uri_params::type>>> http_session::_requests_metadata
{
    {
        {"/api/user/login", http::verb::post, uri_params::type::no},
        {true, jwt_token_type::no, request_handlers::user::login}
    },
    {
        {"/api/user/logout", http::verb::post, uri_params::type::no},
        {false, jwt_token_type::refresh_token, request_handlers::user::logout}
    },
    {
        {"/api/user/tokens", http::verb::put, uri_params::type::no},
        {false, jwt_token_type::refresh_token, request_handlers::user::refresh_tokens}
    },
    {
        {"/api/user/sessions", http::verb::get, uri_params::type::no},
        {false, jwt_token_type::refresh_token, request_handlers::user::get_sessions_info}
    },
    {
        {"/api/user/sessions", http::verb::delete_, uri_params::type::path},
        {false, jwt_token_type::access_token, request_handlers::user::close_session}
    },
    {
        {"/api/user/sessions", http::verb::delete_, uri_params::type::no},
        {false, jwt_token_type::refresh_token, request_handlers::user::close_all_sessions_except_current}
    },
    {
        {"/api/user/password", http::verb::put, uri_params::type::no},
        {true, jwt_token_type::refresh_token, request_handlers::user::change_password}
    },
    {
        {"/api/file_system/folders", http::verb::get, uri_params::type::no},
        {false, jwt_token_type::access_token, request_handlers::file_system::get_folders_info}
    },
    {
        {"/api/file_system/folders", http::verb::post, uri_params::type::no},
        {true, jwt_token_type::access_token, request_handlers::file_system::create_folder}
    },
    {
        {"/api/file_system/folders", http::verb::delete_, uri_params::type::query},
        {false, jwt_token_type::access_token, request_handlers::file_system::delete_folders}
    },
    {
        {"/api/file_system/folders", http::verb::patch, uri_params::type::path},
        {true, jwt_token_type::access_token, request_handlers::file_system::rename_folder}
    },
    {
        {"/api/file_system/files", http::verb::get, uri_params::type::query},
        {false, jwt_token_type::access_token, request_handlers::file_system::get_files_info}
    },
    {
        {"/api/file_system/files", http::verb::post, uri_params::type::query},
        {true, jwt_token_type::access_token, [](const request_params&, response_params&){}}
    },
    {
        {"/api/file_system/files", http::verb::delete_, uri_params::type::query},
        {false, jwt_token_type::access_token, request_handlers::file_system::delete_files}
    },
    {
        {"/api/file_system/files", http::verb::patch, uri_params::type::path},
        {true, jwt_token_type::access_token, request_handlers::file_system::rename_file}
    }
};

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
    // The error means that the timer on the logical operation(write/read) is expired
    if (error_code == beast::error::timeout)
    {
        return;
    }

    // The error means that client can't make correct ssl handshake or just send random data packets
    if (error_code)
    {
        return do_close();
    }

    set_request_props();
    do_read_header();
}

void http_session::set_request_props()
{
    // Make the request parser empty before reading new request,
    // otherwise the operation behavior is undefined
    _request_parser.emplace();

    // Erase previous Set-Cookie field values
    _response.erase(http::field::set_cookie);
    // Clear previous body data
    _response.body().clear();

    // Init response params with empty values(error status with unknown status code) to use it in request handler
    _response_params.init_params();
    
    // Set unlimited body to prevent "body limit exceeded" error
    // and handle the real limit in validate_request_attributes as it has to be processed differently
    // depending on if the request is for uploading files or not
    _request_parser->body_limit(boost::none);

    // Clear the buffer for each request
    _buffer.clear();
}

void http_session::do_read_header()
{
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
    if (error_code == http::error::end_of_stream || error_code == asio::ssl::error::stream_truncated)
    {
        return do_close();
    }

    // The error means that the timer on the logical operation(write/read) is expired
    if (error_code == beast::error::timeout)
    {
        return;
    }

    // Request has invalid header structure so it can't be processed
    if (error_code)
    {
        prepare_error_response(
            http::status::bad_request, 
            "Invalid request");
        return do_write_response(false);
    }   

    // Reset the timeout
    beast::get_lowest_layer(_stream).expires_never();

    // Process OPTIONS method separately as it is necessary for correct browser CORS policy work
    if (_request_parser->get().method() == http::verb::options)
    {
        parse_response_params();

        return do_write_response(true);
    }

    // Determine the handler to invoke by request metadata
    // or construct error response if didn't found corresponding request
    if (auto request_metadata = _requests_metadata.find(
        {
            uri_params::get_unparameterized_uri(_request_parser->get().target()), 
            _request_parser->get().method(),
            uri_params::determine_uri_parameters_type(_request_parser->get().target())
        }); 
        request_metadata != _requests_metadata.end())
    {
        // Determine if the request is for uploading files as it has to be processed separately
        bool is_uploading_files_request = 
            request_metadata->first == 
                std::tuple<std::string_view, http::verb, uri_params::type>{
                    "/api/file_system/files", http::verb::post, uri_params::type::query};

        // Check if the request attributes meets the requirements depending on the expected body presence
        if (!validate_request_attributes(std::get<0>(request_metadata->second), is_uploading_files_request))
        {
            return do_write_response(false);
        }

        // Parse request to get necessary http params in _request_params and use it in request handler
        parse_request_params();

        // Validate jwt token(access or refresh) with the presence
        if (!validate_jwt_token(std::get<1>(request_metadata->second)))
        {
            return do_write_response(false);
        }

        // Read the body with the presence and invoke request handler
        if (std::get<0>(request_metadata->second))
        {
            // Process uploading files separately because it is not synchronous as other handlers 
            if (is_uploading_files_request)
            {
                return upload_files();
            }

            do_read_body(std::get<2>(request_metadata->second));
        }
        else
        {
            // Invoke request handler to process corresponding request logic
            std::get<2>(request_metadata->second)(_request_params, _response_params);

            // Parse reponse params to set all of the necessary fields in the _response
            parse_response_params();

            do_write_response(true);
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
    if (error_code == http::error::end_of_stream || error_code == asio::ssl::error::stream_truncated)
    {
        return do_close();
    }

    // The error means that the timer on the logical operation(write/read) is expired
    if (error_code == beast::error::timeout)
    {
        return;
    }

    // Request has invalid body structure so it can't be processed
    if (error_code)
    {
        prepare_error_response(
            http::status::bad_request, 
            "Invalid request");
        return do_write_response(false);
    }   
    
    // Reset the timeout as we may do long request handling 
    beast::get_lowest_layer(_stream).expires_never();

    // Invoke the corresponding request handler to process the request logic
    request_handler(_request_params, _response_params);

    // Parse reponse params to set all of the necessary fields in the _response
    parse_response_params();

    do_write_response(true);
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
        return;
    }

    if (error_code)
    {
        return do_close();
    }

    // Determine either we have to read the next request or close the connection
    // depending on either the response was regular or error
    if (keep_alive)
    {
        set_request_props();    
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

void http_session::prepare_error_response(http::status response_status, std::string_view error_message)
{
    _response.result(response_status);
    _response.body() = json::serialize(
        json::object
        {
            {"error", error_message}
        });
    _response.prepare_payload();
}

void http_session::parse_request_params()
{
    _request_params.uri = _request_parser->get().target();
    _request_params.user_agent = _request_parser->get()[http::field::user_agent];
    _request_params.user_ip = _request_parser->get()["X-Forwarded-For"];

    // Get access token from the Authorization field in the http header in the format of "Bearer <token>"
    // so access token are less than the length 8 can't be at all(don't try to predict access token length) 
    if (_request_parser->get()[http::field::authorization].size() >= 8) 
    {
        _request_params.access_token = _request_parser->get()[http::field::authorization].substr(7);    
    }

    // Get the refresh token from the Cookie field in the http header 
    // by parsing cookies and getting the value of the key "refreshToken"
    _request_params.refresh_token = cookie_utils::get_cookie_value(
        _request_parser->get()[http::field::cookie], 
        "refreshToken");

    // Get the remember me option from the Cookie field in the http header 
    // by parsing cookies and getting the value of the key "rememberMe" and converting string to bool
    _request_params.remember_me = cookie_utils::get_cookie_value(
        _request_parser->get()[http::field::cookie], 
        "rememberMe") == "true" ? true : false;
}

void http_session::parse_response_params()
{
    // If error status is unknown then request was successfully handled and the status has to be OK(200)  
    if (_response_params.error_status == http::status::unknown)
    {
        _response.result(http::status::ok);
    }
    else
    {
        _response.result(_response_params.error_status);
    }

    // If refresh token is not empty then set cookies with refreshToken and rememberMe fields respectively
    if (_response_params.refresh_token != "")
    {
        _response.insert(
            http::field::set_cookie, 
            cookie_utils::set_cookie(
                "refreshToken", 
                _response_params.refresh_token, 
                _response_params.max_age));
        _response.insert(
            http::field::set_cookie,
            cookie_utils::set_cookie(
                "rememberMe", 
                _response_params.remember_me ? "true" : "false", 
                _response_params.max_age));
    }

    // Prepare payload by setting the Content-Length and Transfer-Encoding fields
    _response.prepare_payload();
}

bool http_session::validate_request_attributes(bool has_body, bool is_uploading_files_request)
{
    // Request with body
    if (has_body)
    {
        // We expect body but there is no
        if (_request_parser->is_done())
        {
            prepare_error_response(
                http::status::unprocessable_entity, 
                "Body was expected");
            return false;
        }
        
        // Content-Length is necessary to properly parse the body
        if (!_request_parser->content_length())
        {
            prepare_error_response(
                http::status::length_required, 
                "No Content-Length field");
            return false;
        }

        // Forbid requests with body size more than 1 MB if it is not for uploading files
        if (!is_uploading_files_request && *_request_parser->content_length() > 1024 * 1024)
        {
            prepare_error_response(
                http::status::payload_too_large, 
                "Too large body size");
            return false;
        }

        return true;
    }
    // Request with no body
    else
    {
        // We don't expect body but they is there
        if (!_request_parser->is_done())
        {
            prepare_error_response(
                http::status::unprocessable_entity, 
                "No body was expected");
            return false;
        }

        return true;
    }
}

bool http_session::validate_jwt_token(jwt_token_type token_type)
{
    switch (token_type)
    {
        case (jwt_token_type::no):
        {
            return true;
        }

        case (jwt_token_type::access_token):
        {
            if (!jwt_utils::is_token_valid(std::string{_request_params.access_token}))
            {
                prepare_error_response(http::status::unauthorized, "Invalid access token");
                return false;
            }

            return true;
        }

        case (jwt_token_type::refresh_token):
        {
            if (!jwt_utils::is_token_valid(std::string{_request_params.refresh_token}))
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

void http_session::upload_files()
{
    size_t folder_id;

    if (!uri_params::get_query_parameters(_request_parser->get().target(), "folderId", folder_id))
    {
        prepare_error_response(
            http::status::unprocessable_entity, 
            "Invalid folder id");
        return do_write_response(false);
    }

    database_connection_wrapper db_conn = database_connections_pool::get();

    // No available connections
    if (!db_conn)
    {
        prepare_error_response(
            http::status::internal_server_error, 
            "No available database connections");
        return do_write_response(false);
    }

    std::optional<bool> does_folder_exist_opt = db_conn->check_folder_existence_by_id(folder_id);
    
    // An error occured with database connection
    if (!does_folder_exist_opt.has_value())
    {
        prepare_error_response(
            http::status::internal_server_error, 
            "Internal server error occured");
        return do_write_response(false);
    }

    // Folder with folder_id doesn't exist
    if (!does_folder_exist_opt.value())
    {
        prepare_error_response(
            http::status::unprocessable_entity, 
            "Invalid folder id");
        return do_write_response(false);
    }
    
    // Transfer data from the main buffer to our string buffer as some part of body 
    // could be previously obtained while reading the header
    _files_string_buffer.assign(asio::buffer_cast<const char*>(_buffer.data()), _buffer.size());

    // Use special wrapper for string as buffer for uploading files
    // Main buffer of type beast::flat_buffer is incompatible here(works weird)
    // Set the maximum size of the uploading packet to 10 MB
    dynamic_buffer buffer{_files_string_buffer, 1024 * 1024 * 10};

    std::string_view content_type = _request_parser->get()[http::field::content_type];
    size_t boundary_position = content_type.find("boundary=");

    // Boundary was not found in the content type
    if (boundary_position == std::string::npos)
    {
        prepare_error_response(
            http::status::unprocessable_entity, 
            "Invalid Content-Type");
        return do_write_response(false);
    }

    // Determine the boundary for multipart/form-data content type
    std::string_view boundary = content_type.substr(boundary_position + 9);

    // Declare file variable to write obtained data to the corresponding files 
    std::ofstream file;

    // Store the ids and paths for all uploaded files to process them afterward(e.g. to recode or unzip for archives)
    std::list<std::pair<size_t, std::string>> file_ids_and_paths;

    size_t user_id;

    // This error means that access token was somehow generated incorrectly
    if (!jwt_utils::get_token_claim(
        std::string{_request_params.access_token}, 
        "userId", 
        user_id))
    {
        prepare_error_response(
            http::status::unauthorized, 
            "Invalid access token");
        return do_write_response(false);
    }

    // Set the timeout
    beast::get_lowest_layer(_stream).expires_after(std::chrono::seconds(30));

    // Read the boundary before the first header of file 
    asio::async_read_until(_stream, buffer, boundary, 
        beast::bind_front_handler(
            [this, self = shared_from_this()](
                dynamic_buffer&& buffer, 
                std::string_view&& boundary,
                size_t user_id,
                size_t folder_id,
                std::ofstream&& file, 
                database_connection_wrapper&& db_conn,
                std::list<std::pair<size_t, std::string>>&& file_ids_and_paths,
                beast::error_code error_code, std::size_t bytes_transferred)
            {
                if (error_code)
                {
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
                        &http_session::process_uploading_file_header, 
                        shared_from_this(), 
                        std::move(buffer), 
                        std::move(boundary), 
                        user_id,
                        folder_id, 
                        std::move(file),
                        std::move(db_conn),
                        std::move(file_ids_and_paths)));
            }, 
            std::move(buffer), 
            std::move(boundary), 
            user_id,
            folder_id, 
            std::move(file),
            std::move(db_conn),
            std::move(file_ids_and_paths)));
}

void http_session::process_uploading_file_header(
    dynamic_buffer&& buffer, 
    std::string_view&& boundary,
    size_t user_id,
    size_t folder_id,
    std::ofstream&& file,
    database_connection_wrapper&& db_conn, 
    std::list<std::pair<size_t, std::string>>&& file_ids_and_paths,
    beast::error_code error_code, std::size_t bytes_transferred)
{
    if (error_code)
    {
        // If there are uploaded files then start separately processing them and close the connection
        if (!file_ids_and_paths.empty())
        {
            std::thread{
                request_handlers::file_system::process_uploaded_files,
                std::move(file_ids_and_paths), 
                user_id,
                folder_id, 
                std::move(db_conn)}.detach();
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
        // If there are uploaded files then start separately processing them and return response
        if (!file_ids_and_paths.empty())
        {
            std::thread{
                request_handlers::file_system::process_uploaded_files,
                std::move(file_ids_and_paths), 
                user_id,
                folder_id, 
                std::move(db_conn)}.detach();
        }

        prepare_error_response(
            http::status::unprocessable_entity, 
            "No filename field found");
        return do_write_response(false);
    }

    // Shift to the filename itself(e.g. filename="file.txt")
    file_name_position += 10;

    std::string file_name = std::string{string_buffer.substr(
        file_name_position, string_buffer.find('"', file_name_position) - file_name_position)};
    file_name_position = file_name.find_last_of('.');
    std::string file_extension = file_name.substr(file_name_position + 1);
    file_name = file_name.substr(0, file_name_position);

    // Invalid file extension
    if (!config::ALLOWED_UPLOADING_FILE_EXTENSIONS.contains(file_extension))
    {
        // If there are uploaded files then start separately processing them and return response
        if (!file_ids_and_paths.empty())
        {
            std::thread{
                request_handlers::file_system::process_uploaded_files,
                std::move(file_ids_and_paths), 
                user_id,
                folder_id, 
                std::move(db_conn)}.detach();
        }

        prepare_error_response(
            http::status::unprocessable_entity, 
            "Filename contains invalid extension ." + std::string{file_extension});
        return do_write_response(false);
    }

    // Trim whitespaces at the beggining and at the end of the file name
    boost::algorithm::trim(file_name);

    // Check if the string is empty after the trim
    // In this case assign it with "(1)" like it is the copy of empty string 
    // because it is forbidden to have empty file name
    if (file_name.empty())
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

    std::optional<bool> does_file_exist_opt;
    size_t opening_bracket, copy_number, non_digit_pos;

    // Check if the file with specified name exists in database
    // If so we create names with copies by adding number after it: test.txt -> test(1).txt -> test(2).txt
    // until the modified name is available in database
    while (true)    
    {
        does_file_exist_opt = db_conn->check_file_existence_by_name(folder_id, file_name);

        if (!does_file_exist_opt.has_value())
        {
            // If there are uploaded files then start separately processing them and return response
            if (!file_ids_and_paths.empty())
            {
                std::thread{
                    request_handlers::file_system::process_uploaded_files,
                    std::move(file_ids_and_paths), 
                    user_id,
                    folder_id, 
                    std::move(db_conn)}.detach();
            }
            
            prepare_error_response(
                http::status::internal_server_error, 
                "Internal server error occured");
            return do_write_response(false);
        }

        // We found the available file name so stop creating copies
        if (!does_file_exist_opt.value())
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

    std::optional<std::pair<size_t, std::string>> file_id_and_path_opt = db_conn->insert_uploading_file(
        user_id,
        folder_id,
        file_name,
        file_extension);

    // An error occured with database connection
    if (!file_id_and_path_opt.has_value())
    {
        // If there are uploaded files then start separately processing them and return response
        if (!file_ids_and_paths.empty())
        {
            std::thread{
                request_handlers::file_system::process_uploaded_files,
                std::move(file_ids_and_paths), 
                user_id,
                folder_id, 
                std::move(db_conn)}.detach();
        }
        
        prepare_error_response(
            http::status::internal_server_error, 
            "Internal server error occured");
        return do_write_response(false);
    }

    // Create and open the file to write the obtaining data
    file.open(file_id_and_path_opt->second, std::ios::binary);

    // Store id and path of the current file to process it after the upload(e.g. to recode or unzip for archives)
    file_ids_and_paths.emplace_back(file_id_and_path_opt.value());

    // Consume the file header bytes 
    buffer.consume(bytes_transferred);

    // Set the timeout
    beast::get_lowest_layer(_stream).expires_after(std::chrono::seconds(30));

    // Read the file data obtaining bytes until the boundary that represents the end of file
    asio::async_read_until(_stream, buffer, boundary, 
        beast::bind_front_handler( 
            &http_session::process_uploading_file_data, 
            shared_from_this(), 
            std::move(buffer), 
            std::move(boundary), 
            user_id,
            folder_id, 
            std::move(file),
            std::move(db_conn),
            std::move(file_ids_and_paths)));
}

void http_session::process_uploading_file_data(
    dynamic_buffer&& buffer, 
    std::string_view&& boundary,
    size_t user_id,
    size_t folder_id,
    std::ofstream&& file,
    database_connection_wrapper&& db_conn, 
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
                &http_session::process_uploading_file_data, 
                shared_from_this(), 
                std::move(buffer), 
                std::move(boundary), 
                user_id,
                folder_id, 
                std::move(file),
                std::move(db_conn),
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
        db_conn->delete_file(file_ids_and_paths.back().first);

        // Remove the file from the list of uploaded files 
        file_ids_and_paths.pop_back();

        // If there are uploaded files then start separately processing them and return response
        if (!file_ids_and_paths.empty())
        {
            std::thread{
                request_handlers::file_system::process_uploaded_files,
                std::move(file_ids_and_paths), 
                user_id,
                folder_id, 
                std::move(db_conn)}.detach();
        }
    };

    // Unexpected error occured so clean up everything about not uploaded file
    if (error_code)
    {
        file.close();

        process_file_cleanup();

        return do_close();
    }

    // Reset the timeout
    beast::get_lowest_layer(_stream).expires_never();

   // Write obtained bytes to the file excluding CRLF after the file data and -- followed by boundary
   // -- is the part of the boundary, used only in body, so we have to consider this -- length because
   // boudary variable doesn't contain it
    file.write(asio::buffer_cast<const char*>(buffer.data()), bytes_transferred - boundary.size() - 4);

    // Close the file as its uploading is over
    file.close();

    // Consume obtained bytes
    buffer.consume(bytes_transferred); 

    size_t file_size;

    // Determine the file size of just uploaded file
    try
    {
        file_size = std::filesystem::file_size(file_ids_and_paths.back().second);
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();

        process_file_cleanup();
   
        prepare_error_response(
            http::status::internal_server_error, 
            "Internal server error occured");
        return do_write_response(false);
    }

    // Update the data about just uploaded file
    if (!db_conn->update_uploaded_file(file_ids_and_paths.back().first, file_size).has_value())
    {
        process_file_cleanup();
   
        prepare_error_response(
            http::status::internal_server_error, 
            "Internal server error occured");
        return do_write_response(false);
    }

    // If there is "--" after the boundary then there are no more files and request body is over
    // So start processing uploaded files and send response
    if (std::string_view{asio::buffer_cast<const char*>(buffer.data()), buffer.size()} == "--\r\n")
    {
        std::thread{
            request_handlers::file_system::process_uploaded_files,
            std::move(file_ids_and_paths), 
            user_id,
            folder_id, 
            std::move(db_conn)}.detach();

        _response.result(http::status::ok);
        _response.prepare_payload();

        return do_write_response(true);
    }
    
    // Set the timeout
    beast::get_lowest_layer(_stream).expires_after(std::chrono::seconds(30));
    
    // Read the next file header
    asio::async_read_until(_stream, buffer, "\r\n\r\n", 
        beast::bind_front_handler( 
            &http_session::process_uploading_file_header, 
            shared_from_this(), 
            std::move(buffer), 
            std::move(boundary), 
            user_id,
            folder_id, 
            std::move(file),
            std::move(db_conn),
            std::move(file_ids_and_paths)));
}