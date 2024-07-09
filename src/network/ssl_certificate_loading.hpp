#ifndef SERVER_CERTIFICATE_HPP
#define SERVER_CERTIFICATE_HPP

#include <boost/asio/ssl/context.hpp>
#include <cstddef>
#include <memory>

#include <config.hpp>

inline void load_ssl_certificate(boost::asio::ssl::context& ctx)
{    
    ctx.set_options(
        boost::asio::ssl::context::default_workarounds |
        boost::asio::ssl::context::no_sslv2 |
        boost::asio::ssl::context::no_sslv3 |
        boost::asio::ssl::context::no_tlsv1_1);

    // Set ssl certificate
    ctx.use_certificate_chain_file(config::ssl_cert_path);

    // Set ssl key
    ctx.use_private_key_file(config::ssl_key_path, boost::asio::ssl::context::file_format::pem);
}

#endif