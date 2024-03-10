#ifndef REQUEST_AND_RESPONSE_PARAMS_HPP
#define REQUEST_AND_RESPONSE_PARAMS_HPP

#include <string>
#include <chrono>
#include <boost/beast/http/status.hpp>

struct request_params
{
    std::string_view uri{};
    std::string_view uri_template{};
    std::string user_ip{};
    std::string_view user_agent{};
    std::string_view access_token{};
    std::string_view refresh_token{};
    bool remember_me{};
    std::string& body;
};

struct response_params
{
    boost::beast::http::status status{};
    std::string refresh_token{};
    bool remember_me{};
    std::chrono::seconds max_age{};
    std::string& body;

    void init_params()
    {
        status = boost::beast::http::status::ok;
        std::string refresh_token = {};
        bool remember_me = {};
        std::chrono::seconds max_age = {};
    }
};

#endif