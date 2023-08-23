#include <network/http_session.hpp>
#include <request_handlers/request_handlers.hpp>

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
    net::dispatch(
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

    // The error means that client closed the connection 
    if(error_code == http::error::end_of_stream)
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

    // Reset the timeout???
    beast::get_lowest_layer(_stream).expires_never();

    // Determine the handler to invoke by request target 
    // or construct error response if didn't found corresponding target
    if (auto target_handler = _target_to_handler_relations.find(_request_parser->get().target()); 
        target_handler != _target_to_handler_relations.end())
    {
        target_handler->second();
    }
    else
    {
        prepare_error_response(
            http::status::not_found, 
            "Route " + std::string(_request_parser->get().target()) + " не найден");
        do_write_response(false);
    }
}

void http_session::do_read_body(const std::function<void(
            const http::request_parser<http::string_body>&, 
            http::response<http::string_body>&)>& request_handler)
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
    const std::function<void(
        const http::request_parser<http::string_body>&, 
        http::response<http::string_body>&)>& request_handler, 
    beast::error_code error_code, 
    std::size_t bytes_transferred)
{
    // Suppress compiler warnings about unused variable bytes_transferred  
    boost::ignore_unused(bytes_transferred);

    // The error means that client closed the connection
    if(error_code == http::error::end_of_stream)
        return do_close();

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

        // Forbid requests with body size more than 1 MB(there is exception for uploading requests)
        if (*_request_parser->content_length() > 1024 * 1024)
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
        // We don't expect body octets but they are there
        if (!_request_parser->is_done())
        {
            prepare_error_response(
                http::status::unprocessable_entity, 
                "No body octets were expected");
            return false;
        }
        return true;
    }
}

bool http_session::validate_access_token()
{
    if (!jwt_utils::is_token_valid(_request_parser->get()[http::field::authorization]))
    {
        prepare_error_response(http::status::unauthorized, "Неверный accessToken");
        return false;
    }

    return true;
}

bool http_session::validate_refresh_token()
{
    if (!jwt_utils::is_token_valid(std::string{cookie_utils::get_cookie_value(
        _request_parser->get()[http::field::cookie], 
        "refreshToken")}))
    {
        prepare_error_response(http::status::unauthorized, "Неверный refreshToken");
        return false;
    }

    return true;
}