#ifndef LISTENER_HPP
#define LISTENER_HPP

//local
#include <logging/logger.hpp>
#include <network/http_session.hpp>

//internal
#include <memory>

//external
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>

namespace beast = boost::beast;        
namespace asio = boost::asio;            
namespace ssl = boost::asio::ssl;       
using tcp = boost::asio::ip::tcp;

class listener : public std::enable_shared_from_this<listener>
{
    public:
        listener(asio::io_context& io_context, ssl::context& ssl_context, tcp::endpoint endpoint);
            
        void run();

    private:
        void do_accept();

        void on_accept(beast::error_code error_code, tcp::socket socket);

        asio::io_context& _io_context;
        ssl::context& _ssl_context;
        tcp::acceptor _acceptor;
};

#endif