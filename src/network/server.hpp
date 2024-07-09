#ifndef SERVER_HPP
#define SERVER_HPP

//local
#include <config.hpp>
#include <network/listener.hpp>
#include <network/ssl_certificate_loading.hpp>
#include <database/database_connections_pool.hpp>

//internal
#include <thread>
#include <memory>

//external
#include <boost/beast/ssl.hpp>
#include <boost/asio/signal_set.hpp>

namespace asio = boost::asio;    
namespace ssl = boost::asio::ssl;     
using tcp = boost::asio::ip::tcp; 

namespace server
{
    // Run the server with parameters specified in the config file
    inline void run()
    {
        try
        {
            // Initialize config variables from the config file
            config::init();

            // Initialize pool of database connections
            database_connections_pool::init(
                config::database_connections_number,
                config::database_username,
                config::database_password,
                "127.0.0.1",
                config::database_port,
                config::database_name);

            // The io_context is required for all I/O
            asio::io_context io_context{config::threads_number};

            // The SSL context is required, and holds certificates
            ssl::context ssl_context{ssl::context::tlsv12};

            // Load and set server certificate to enable ssl
            load_ssl_certificate(ssl_context);        

            // Create and launch a listening port
            std::make_shared<listener>(
                io_context,
                ssl_context,
                tcp::endpoint{asio::ip::make_address(config::server_ip_address), config::server_port})->run();

            // Capture SIGINT and SIGTERM to perform a clean shutdown
            asio::signal_set signals(io_context, SIGINT, SIGTERM);
            signals.async_wait(
                [&io_context](const beast::error_code&, int)
                {
                    // Stop the io_context. This will cause run()
                    // to return immediately, eventually destroying the
                    // io_context and all of the sockets in it
                    io_context.stop();
                });

            // Run the I/O service on the requested number of threads
            std::vector<std::thread> thread_pool;
            thread_pool.reserve(config::threads_number - 1);
            for (auto i = config::threads_number - 1; i > 0; --i)
            {
                thread_pool.emplace_back(
                    [&io_context]
                    {
                        io_context.run();
                    });   
            }

            LOG_INFO << "The server was successfully started!";

            io_context.run();

            // (If we get here, it means we got a SIGINT or SIGTERM)
            // Block until all the threads exit
            for (auto& thread : thread_pool)
                thread.join();

            LOG_INFO << "The server was successfully shut down!";
        }
        catch (const std::exception& ex)
        {
            LOG_ERROR << ex.what();
            LOG_INFO << "The server was unexpectedly shut down due to an error!";
        }
    }
}

#endif