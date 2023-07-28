#include <network/http_session.hpp>

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
    beast::get_lowest_layer(_stream).expires_after(
        std::chrono::seconds(30));
    
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
    // Make the request empty before reading,
    // otherwise the operation behavior is undefined.
    _header_request_parser.emplace();

    // Set the timeout.
    beast::get_lowest_layer(_stream).expires_after(std::chrono::seconds(30));
    
    // Read a request header
    http::async_read(_stream, _buffer, *_header_request_parser,
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

    // Determine the function to invoke by request target or send error response if didn't found corresponding target
    if (auto target_function = _target_to_function_relations.find(_header_request_parser->get().target()); 
        target_function != _target_to_function_relations.end())
    {
        target_function->second();
    }
    else
    {
        send_error_response(
            http::status::not_found, 
            "Target " + std::string(_header_request_parser->get().target()) + " не найден");
    }
    
}

void http_session::send_error_response(http::status response_status, const std::string& error_message)
{
    _string_response.result(response_status);
    _string_response.body() = R"({"error":")" + error_message + R"("})";
    _string_response.prepare_payload();

    // Write the error response
    http::async_write(
        _stream,
        _string_response,
        beast::bind_front_handler(
            &http_session::on_write,
            this->shared_from_this()));
}

void http_session::send_response(http::message_generator&& msg)
{
    // Write the response
    http::async_write(
        _stream,
        _string_response,
        beast::bind_front_handler(
            &http_session::on_write,
            this->shared_from_this()));
}

void http_session::on_write(beast::error_code error_code, std::size_t bytes_transferred)
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

void http_session::handle_header()
{
    std::string_view target = _header_request_parser->get().target();
    boost::tokenizer<boost::char_separator<char>> target_tokens(target, boost::char_separator<char>("/"));
    for (auto token_iterator = target_tokens.begin();  token_iterator != target_tokens.end(); ++token_iterator)
    {
        //_target_tokens.at("");
    }
}

void http_session::handle_login()
{

}

void http_session::handle_logout()
{

}

void http_session::handle_refresh_web()
{
    
}

void http_session::handle_refresh_desktop()
{

}