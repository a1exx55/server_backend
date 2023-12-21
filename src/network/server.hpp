#ifndef SERVER_HPP
#define SERVER_HPP

//local
#include <config.hpp>
#include <network/listener.hpp>
#include <network/server_certificate.hpp>
#include <database/database_pool.hpp>
//#include <boost/certify/extensions.hpp>
//#include <boost/certify/https_verification.hpp>

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
    // Run the server with parameters specified in config.hpp file
    inline void run()
    {
        try
        {
            // Initialize pool of database connections
            database_pool::init(
                config::DATABASES_NUMBER,
                config::DATABASE_USERNAME,
                config::DATABASE_PASSWORD,
                config::SERVER_IP_ADDRESS,
                config::DATABASE_PORT,
                config::DATABASE_NAME);
        }
        catch(const std::exception& ex)
        {
            LOG_ERROR << ex.what();
            return;
        }
        

        // The io_context is required for all I/O
        asio::io_context io_context{config::THREADS_NUMBER};

        // The SSL context is required, and holds certificates
        ssl::context ssl_context{ssl::context::tlsv12};

        // This holds the self-signed certificate used by the server

        try
        {
            load_server_certificate(ssl_context);        
        }
        catch(const std::exception& ex)
        {
            LOG_ERROR << ex.what();
            return;
        }

        //???maybe will be necessary to perform real tcp
        //ssl_context.set_verify_mode(ssl::verify_peer);
        //ssl_context.set_verify_mode(ssl::context::verify_peer );
        //boost::certify::enable_native_https_server_verification(ssl_context);

        // Create and launch a listening port
        std::make_shared<listener>(
            io_context,
            ssl_context,
            tcp::endpoint{asio::ip::make_address(config::SERVER_IP_ADDRESS), config::SERVER_PORT})->run();

        // Capture SIGINT and SIGTERM to perform a clean shutdown
        asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait(
            [&io_context](beast::error_code const&, int)
            {
                // Stop the `io_context`. This will cause `run()`
                // to return immediately, eventually destroying the
                // `io_context` and all of the sockets in it.
                io_context.stop();
            });

        // Run the I/O service on the requested number of threads
        std::vector<std::thread> thread_pool;
        thread_pool.reserve(config::THREADS_NUMBER - 1);
        for (auto i = config::THREADS_NUMBER - 1; i > 0; --i)
        {
            thread_pool.emplace_back(
                [&io_context]
                {
                    io_context.run();
                });   
        }
        io_context.run();

        // (If we get here, it means we got a SIGINT or SIGTERM)
        // Block until all the threads exit
        for (auto& thread : thread_pool)
            thread.join();
    }
}

#endif