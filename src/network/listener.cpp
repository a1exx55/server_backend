#include <network/listener.hpp>

listener::listener(net::io_context& io_context, ssl::context& ssl_context, tcp::endpoint endpoint)
    :_io_context(io_context), _ssl_context(ssl_context), _acceptor(io_context)
{
    beast::error_code error_code;
    
    _acceptor.open(endpoint.protocol(), error_code);
    if (error_code)
    {
        LOG_ERROR << error_code.message();
        return;
    }

    _acceptor.set_option(net::socket_base::reuse_address(true), error_code);
    if (error_code)
    {
        LOG_ERROR << error_code.message();
        return;
    }

    _acceptor.bind(endpoint, error_code);
    if (error_code)
    {
        LOG_ERROR << error_code.message();
        return;
    }

    _acceptor.listen(net::socket_base::max_listen_connections, error_code);
    if (error_code)
    {
        LOG_ERROR << error_code.message();
        return;
    }
}

void listener::run()
{
    do_accept();
}

void listener::do_accept()
{
    // Use make_strand to simplify work with access to shared resourses while using multiple threads
    _acceptor.async_accept(
        net::make_strand(_io_context),
        beast::bind_front_handler(
            &listener::on_accept,
            shared_from_this()));
}

void listener::on_accept(beast::error_code error_code, tcp::socket socket)
{
    if (error_code)
    {
        LOG_ERROR << error_code.message();
        return;
    }
    else
    {
        // Create the session and run it
        std::make_shared<http_session>(
            std::move(socket),
            _ssl_context)->run();
    }

    // Accept another connection
    do_accept();
}