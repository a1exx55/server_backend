#ifndef COOKIE_UTILS
#define COOKIE_UTILS

//internal
#include <string>
#include <chrono>

//external
#include <boost/json.hpp>

namespace json = boost::json;

namespace cookie_utils
{
    // Return the string representing the value for the Set-Cookie field in the format of 
    // "key=value; Max-Age=max_age; Path=/; HttpOnly; Secure" (HttpOnly and Secure are set by default)
    // If max_age is -1 then corresponding field is absent to set cookie lifetime up to current session
    std::string set_cookie(
        std::string_view key, 
        std::string_view value, 
        std::chrono::seconds max_age = std::chrono::seconds{-1});

    // Parse cookies "key1=value1; key2=value2..." into corresponding json object
    json::object parse_cookies(std::string_view cookies);

    // Parse cookies and return the value to the corresponding cookie key
    std::string_view get_cookie_value(std::string_view cookies, std::string_view cookie_key);
}

#endif