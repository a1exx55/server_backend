#include <network/http_session.hpp>
#include "http_session.hpp"

http_session::http_session(tcp::socket&& socket, ssl::context& ssl_context)
    :
    _stream(std::move(socket), ssl_context),
    _form_data{_stream, _buffer},
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
    _response.set(http::field::access_control_allow_origin, config::domain_name);
    _response.set(http::field::access_control_allow_methods, "OPTIONS, GET, POST, PUT, PATCH, DELETE");
    _response.set(http::field::access_control_allow_headers, "Content-Type, Authorization");
}

const http_utils::http_endpoints_storage
    <std::tuple<bool, jwt_token_type, request_handler_t>> http_session::_endpoints
{
    {
        "/api/user/login", http::verb::post,
        {true, jwt_token_type::no, request_handlers::user::login}
    },
    {
        "/api/user/logout", http::verb::post,
        {false, jwt_token_type::refresh_token, request_handlers::user::logout}
    },
    {
        "/api/user/tokens", http::verb::put,
        {false, jwt_token_type::refresh_token, request_handlers::user::refresh_tokens}
    },
    {
        "/api/user/sessions", http::verb::get,
        {false, jwt_token_type::refresh_token, request_handlers::user::get_sessions_info}
    },
    {
        "/api/user/sessions/{sessionId}", http::verb::delete_,
        {false, jwt_token_type::access_token, request_handlers::user::close_session}
    },
    {
        "/api/user/sessions", http::verb::delete_,
        {false, jwt_token_type::refresh_token, request_handlers::user::close_all_sessions_except_current}
    },
    {
        "/api/user/password", http::verb::put,
        {true, jwt_token_type::refresh_token, request_handlers::user::change_password}
    },
    {
        "/api/file_system/folders", http::verb::get,
        {false, jwt_token_type::access_token, request_handlers::file_system::get_folders_info}
    },
    {
        "/api/file_system/folders", http::verb::post,
        {true, jwt_token_type::access_token, request_handlers::file_system::create_folder}
    },
    {
        "/api/file_system/folders", http::verb::delete_,
        {false, jwt_token_type::access_token, request_handlers::file_system::delete_folders}
    },
    {
        "/api/file_system/folders/{folderId}", http::verb::patch,
        {true, jwt_token_type::access_token, request_handlers::file_system::rename_folder}
    },
    {
        "/api/file_system/files", http::verb::get,
        {false, jwt_token_type::access_token, request_handlers::file_system::get_files_info}
    },
    {
        "/api/file_system/files/{fileId}/rows_number", http::verb::get,
        {false, jwt_token_type::access_token, request_handlers::file_system::get_file_rows_number}
    },
    {
        "/api/file_system/files/{fileId}/preview/raw", http::verb::get,
        {false, jwt_token_type::access_token, request_handlers::file_system::get_file_raw_rows}
    },
    {
        "/api/file_system/files", http::verb::post,
        {true, jwt_token_type::access_token, [](const request_params&, response_params&){}}
    },
    {
        "/api/file_system/files", http::verb::delete_,
        {false, jwt_token_type::access_token, request_handlers::file_system::delete_files}
    },
    {
        "/api/file_system/files/{fileId}", http::verb::patch,
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
    beast::get_lowest_layer(_stream).expires_after(config::operations_timeout);
    
    // Perform the SSL handshake
    _stream.async_handshake(
        ssl::stream_base::server,
        beast::bind_front_handler(
            &http_session::on_handshake,
            shared_from_this()));
}

void http_session::on_handshake(beast::error_code error_code)
{
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
    beast::get_lowest_layer(_stream).expires_after(config::operations_timeout);
    
    // Read a request header
    http::async_read_header(
        _stream, 
        _buffer, 
        *_request_parser,
        beast::bind_front_handler(
            &http_session::on_read_header,
            shared_from_this()));
}

void http_session::on_read_header(beast::error_code error_code, std::size_t bytes_transferred)
{
    // Suppress compiler warnings about unused variable bytes_transferred  
    boost::ignore_unused(bytes_transferred);

    if (error_code)
    {
        return do_close();
    }

    // Process OPTIONS method separately as it is necessary for correct browser CORS policy work
    if (_request_parser->get().method() == http::verb::options)
    {
        parse_response_params();

        return do_write_response(true);
    }

    // Search for the endpoint by received request's uri and method and get its data
    // or construct error response if didn't found corresponding endpoint
    if (const auto& endpoint = _endpoints.find_endpoint(
            _request_parser->get().target(), 
            _request_parser->get().method()); 
        !endpoint.uri_template.empty())
    {
        // Determine if the request is for uploading files as it has to be processed separately
        bool is_uploading_files_request = 
            endpoint.uri_template == "/api/file_system/files" && endpoint.method == http::verb::post;

        // Check if the request attributes meets the requirements depending on the expected body presence
        if (!validate_request_attributes(std::get<0>(endpoint.metadata), is_uploading_files_request))
        {
            return do_write_response(false);
        }

        // Parse request to get necessary http params in _request_params and use it in request handler
        parse_request_params();

        _request_params.uri_template = endpoint.uri_template;

        // Validate jwt token(access or refresh) with the presence
        if (!validate_jwt_token(std::get<1>(endpoint.metadata)))
        {
            return do_write_response(false);
        }

        // Read the body with the presence and invoke request handler
        if (std::get<0>(endpoint.metadata))
        {
            // Process uploading files separately because it is not synchronous as other handlers 
            if (is_uploading_files_request)
            {
                do_read_uploading_files();
            }
            else
            {
                do_read_body(std::get<2>(endpoint.metadata));
            }
        }
        else
        {
            // Invoke request handler to process corresponding request logic
            std::get<2>(endpoint.metadata)(_request_params, _response_params);

            // Parse response params to set all of the necessary fields in the _response
            parse_response_params();

            do_write_response(true);
        }
    }
    else
    {
        prepare_error_response(
            http::status::not_acceptable, 
            "URI " + std::string(_request_parser->get().target()) + " with method " + 
                std::string(http::to_string(_request_parser->get().method())) + " can't be processed");
        do_write_response(false);
    }
}

void http_session::do_read_body(const request_handler_t& request_handler)
{   
    // Set the timeout.
    beast::get_lowest_layer(_stream).expires_after(config::operations_timeout);
    
    // Read a request header
    http::async_read(
        _stream, 
        _buffer, 
        *_request_parser,
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

    if (error_code)
    {
        return do_close();
    }
    
    // Invoke the corresponding request handler to process the request logic
    request_handler(_request_params, _response_params);

    // Parse response params to set all of the necessary fields in the _response
    parse_response_params();

    do_write_response(true);
}

void http_session::do_read_uploading_files()
{
    size_t folder_id;

    if (!http_utils::uri::get_query_parameter(_request_params.uri, "folderId", folder_id))
    {
        prepare_error_response(
            http::status::unprocessable_entity, 
            "Invalid folder id");
        return do_write_response(true);
    }

    auto db_conn = database_connections_pool::get<file_system_database_connection>();

    // No available connections
    if (!db_conn)
    {
        prepare_error_response(
            http::status::internal_server_error, 
            "No available database connections");
        return do_write_response(true);
    }

    std::optional<bool> does_folder_exist_opt = db_conn->check_folder_existence_by_id(folder_id);
    
    // An error occured with database connection
    if (!does_folder_exist_opt.has_value())
    {
        prepare_error_response(
            http::status::internal_server_error, 
            "Internal server error occured");
        return do_write_response(true);
    }

    // Folder with folder_id doesn't exist
    if (!does_folder_exist_opt.value())
    {
        prepare_error_response(
            http::status::unprocessable_entity, 
            "Invalid folder id");
        return do_write_response(true);
    }

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
        return do_write_response(true);
    }

    _form_data.async_download(
        _request_parser->get()[http::field::content_type], 
        {
            .operations_timeout = config::operations_timeout,
            .on_read_file_header_handler = request_handlers::file_system::process_uploading_file,
            .on_read_file_body_handler = request_handlers::file_system::process_uploaded_file
        }, 
        beast::bind_front_handler(
            &http_session::on_read_uploading_files, 
            shared_from_this()), 
        shared_from_this(),
        std::list<std::tuple<size_t, std::filesystem::path, std::string>>{},
        std::move(user_id),
        std::move(folder_id),
        std::move(db_conn),
        std::ref(_response_params));
}

void http_session::on_read_uploading_files(
    beast::error_code error_code, 
    std::vector<std::filesystem::path>&& file_paths, 
    std::list<std::tuple<size_t, std::filesystem::path, std::string>>&& files_data, 
    size_t user_id, 
    size_t folder_id, 
    database_connection_wrapper<file_system_database_connection>&& db_conn,
    [[maybe_unused]] response_params& response)
{
    if (error_code)
    {
        // Error occurred while uploading file so we have to delete it only from the database ourselves
        // (file is already removed from the filesystem in form-data processing)
        if (file_paths.size() != files_data.size())
        {
            // Delete the file from the database
            db_conn->delete_file(std::get<0>(files_data.back()));

            // Remove the file from the list of uploaded files 
            files_data.pop_back();
        }

        // Process uploaded files in separate thread to avoid blocking in the long synchronous operation
        std::thread{
            request_handlers::file_system::process_uploaded_files,
            std::move(files_data), 
            user_id,
            folder_id, 
            std::move(db_conn)}.detach();
        
        // Error occurred in our handlers so appropriate error is already set in _response_params
        if (error_code == multipart_form_data::error::operation_aborted)
        {
            // Parse response params to set all of the necessary fields in the _response
            parse_response_params();

            return do_write_response(true);
        }

        // Other form-data errors which means invalid request structure
        if (error_code.category() == multipart_form_data::detail::error_codes{})
        {
            prepare_error_response(
                http::status::unprocessable_entity,
                error_code.message());

            return do_write_response(true);
        }

        return do_close();
    }

    // Process uploaded files in separate thread to avoid blocking in the long synchronous operation
    std::thread{
        request_handlers::file_system::process_uploaded_files,
        std::move(files_data), 
        user_id,
        folder_id, 
        std::move(db_conn)}.detach();

    // Parse response params to set all of the necessary fields in the _response
    parse_response_params();

    do_write_response(true);
}

void http_session::do_write_response(bool keep_alive)
{
    // Set the timeout for next operation
    beast::get_lowest_layer(_stream).expires_after(config::operations_timeout);

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
    beast::get_lowest_layer(_stream).expires_after(config::operations_timeout);

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

    _request_params.access_token = _request_parser->get()[http::field::authorization];

    // Get access token from the Authorization field in the http header in the format of "Bearer <token>"
    // so access token are less than the length 8 can't be at all
    if (_request_params.access_token.size() >= 8) 
    {
        _request_params.access_token = _request_params.access_token.substr(7);    
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
    _response.result(_response_params.status);

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
                prepare_error_response(
                    http::status::unauthorized, 
                    "Invalid access token");
                return false;
            }

            return true;
        }
        case (jwt_token_type::refresh_token):
        {
            if (!jwt_utils::is_token_valid(std::string{_request_params.refresh_token}))
            {
                prepare_error_response(
                    http::status::unauthorized, 
                    "Invalid refresh token");
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