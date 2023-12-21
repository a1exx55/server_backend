#ifndef SERVER_CERTIFICATE_HPP
#define SERVER_CERTIFICATE_HPP

#include <boost/asio/buffer.hpp>
#include <boost/asio/ssl/context.hpp>
#include <cstddef>
#include <memory>

#include <config.hpp>

inline void load_server_certificate(boost::asio::ssl::context& ctx)
{    
    ctx.set_password_callback(
        [](std::size_t, boost::asio::ssl::context_base::password_purpose)
        {
            return "";
        });

    ctx.set_options(
        boost::asio::ssl::context::default_workarounds |
        boost::asio::ssl::context::no_sslv2);

    ctx.use_certificate_chain_file(config::SSL_CERT_PATH);

    ctx.use_private_key_file(config::SSL_KEY_PATH, boost::asio::ssl::context::file_format::pem);
}

#endif