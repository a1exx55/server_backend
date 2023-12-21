#include <cookie_utils/cookie_utils.hpp>

std::string cookie_utils::set_cookie(
        std::string_view key, 
        std::string_view value, 
        std::chrono::seconds max_age)
{
    std::string cookie;

    // Reserve estimate number of characters to avoid extra allocations
    cookie.reserve(key.size() + value.size() + 40);

    cookie
        .append(key)
        .append("=")
        .append(value)
        .append("; Max-Age=")
        .append(std::to_string(max_age.count()))
        .append("; HttpOnly; Secure");
    
    return cookie;
}

json::object cookie_utils::parse_cookies(std::string_view cookies)
{
    json::object cookies_json;

    // Reserve predicted number of elements in cookies json(in this project(for performance))
    cookies_json.reserve(2);

    // Use three positions of cookie expression to get the key and value of each cookie:
    // start_position(1) - the start of cookie expression
    // equals_sign_position(2) - the equals sign that is separating key and value
    // semicolon_position(3) - the semicolon that means the end of the cookie 
    // "(1)->key(2)->=value(3)->; ..."
    size_t start_position{0}, equals_sign_position{0}, semicolon_position{0};

    do
    {
        equals_sign_position = cookies.find("=", start_position);
        // The first expression means that there are no more equals signs therefore no cookies to parse
        // The second means that the key is empty that doesn't make sense
        if (equals_sign_position == std::string::npos || equals_sign_position == start_position) 
        {
            return cookies_json;
        }
        semicolon_position = cookies.find(";", equals_sign_position);
        // The key of cookie is substr of positions between start of expression and the equals sign
        // The value is substr of positions between the equals sign and semicolon
        cookies_json[cookies.substr(
            start_position, 
            equals_sign_position - start_position)] = cookies.substr(
                equals_sign_position + 1, 
                semicolon_position - equals_sign_position - 1);
        // The next start_position is semicolon_position + 2 because of space is after semicolon("...; key=value;") 
        start_position = semicolon_position + 2;
    }
    // Do it while there are no more semicolons so no more cookies
    while (semicolon_position != std::string::npos);

    return cookies_json;
}

std::string_view cookie_utils::get_cookie_value(std::string_view cookies, std::string_view cookie_key)
{
    // Find the start position of cookie_key
    size_t cookie_key_position = cookies.find(cookie_key);

    // Return empty object if there are no cookie_key in cookies or 
    // there are no equals sign right after the key(syntax error)
    // Assign cookie_key_position right in the expression to use it later and avoid recalculations
    if (cookie_key_position == std::string::npos || 
        cookies[cookie_key_position = cookie_key_position + cookie_key.size()] != '=')
    {
        return {};
    }

    // Return the substr respresenting the value of corresponding cookie_key -
    // from the equals sign to the semicolon or the end of the string if the latter is absent
    return cookies.substr(
        cookie_key_position + 1, 
        cookies.find(";", cookie_key_position + 1) - cookie_key_position - 1);
}