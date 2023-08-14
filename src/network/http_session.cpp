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
    // Make the request empty before reading new request,
    // otherwise the operation behavior is undefined
    _request_parser.emplace();

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
        return do_close();

    if (error_code)
    {
        LOG_ERROR << error_code.message();
        return;
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
    }

    do_write_response();
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
            shared_from_this(), request_handler));
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

    if (error_code)
    {
        LOG_ERROR << error_code.message();
        return;
    }   

    // Reset the timeout as we may do long request handling 
    beast::get_lowest_layer(_stream).expires_never();

    request_handler(*_request_parser, _response);
}

void http_session::prepare_error_response(http::status response_status, const std::string& error_message)
{
    // Clear unnecessary field
    _response.erase(http::field::set_cookie);
    
    _response.result(response_status);
    _response.body() = R"({"error":")" + error_message + R"("})";
    _response.prepare_payload();
}

void http_session::do_write_response()
{
    // Write the response
    http::async_write(
        _stream,
        _response,
        beast::bind_front_handler(
            &http_session::on_write_response,
            this->shared_from_this()));
}

void http_session::on_write_response(beast::error_code error_code, std::size_t bytes_transferred)
{
    // Suppress compiler warnings about unused variable bytes_transferred  
    boost::ignore_unused(bytes_transferred);

    if (error_code)
    {
        LOG_ERROR << error_code.message();
        return;
    }

    // Read another request
    do_read_header();
}

void http_session::do_close()
{
    // Set the timeout.
    beast::get_lowest_layer(_stream).expires_after(std::chrono::seconds(30));

    // Perform the SSL shutdown
    _stream.async_shutdown(
        beast::bind_front_handler(
            &http_session::on_shutdown,
            shared_from_this()));
}

void http_session::on_shutdown(beast::error_code error_code)
{
    if (error_code)
    {
        LOG_ERROR << error_code.message();
        return;
    }

    // At this point the connection is closed gracefully
}

bool http_session::validate_access_token()
{
    if (!_jwt.is_token_valid(_request_parser->get()[http::field::authorization]))
        prepare_error_response(http::status::unauthorized, "Неверный accessToken");
}

bool http_session::validate_refresh_token()
{
    return false;
}