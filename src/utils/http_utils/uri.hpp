#ifndef HTTP_UTILS_URI_HPP
#define HTTP_UTILS_URI_HPP

#include <string_view>
#include <boost/json/object.hpp>

namespace json = boost::json;

namespace http_utils
{
    namespace uri
    {
        // Get path segments from the uri i.e. uri splitted by slash without query parameters
        // Example: "/api/users/123/login?rememberMe=false" -> {"api", "users", "123", "login"}
        std::vector<std::string_view> get_path_segments(std::string_view uri);

        // Assign the path parameter to the given param_value_to_store, converting it to variable type
        // Parameter is searched for in uri template by its name, 
        // that has to be curly bracketed and assign its actual name in uri
        // Example: if uri="/api/user/123/login", uri_template="/api/user/{userId}/login", 
        // param_name="userId" then param_value_to_store will be 123
        //
        // Return true on successful conversion and assignment, otherwise return false
        template <typename param_value_t>
        bool get_path_parameter(
            std::string_view uri, 
            std::string_view uri_template,
            std::string_view param_name,  
            param_value_t& param_value_to_store)
        {
            size_t param_position_in_uri_template = uri_template.find(param_name);

            if (param_position_in_uri_template == std::string::npos)
            {
                return false;
            }

            size_t slashes_number = std::count(
                uri_template.begin(),
                uri_template.begin() + param_position_in_uri_template,
                '/');

            size_t slash_position = 0;

            for (size_t i = 0; i < slashes_number; ++i)
            {
                slash_position = uri.find('/', slash_position);

                if (slash_position == std::string::npos)
                {
                    return false;
                }

                ++slash_position;
            }

            std::string path_parameter = 
                std::string{uri.substr(slash_position, uri.find('/', slash_position) - slash_position)};

            // Path parameter is a string
            if constexpr (std::is_same_v<param_value_t, std::string>)
            {
                param_value_to_store = path_parameter;
            }
            // Path parameter is a number
            else if constexpr (std::is_integral_v<param_value_t>)
            {
                // Unsigned integers are got from string to uint64_t
                if constexpr (std::is_unsigned_v<param_value_t>)
                {
                    try
                    {
                        param_value_to_store = std::stoull(path_parameter);
                    }
                    // Path parameter string value representation is not a number
                    catch (const std::exception&)
                    {
                        return false;
                    }
                }
                // Signed integers are got from string to int64_t
                else if constexpr (std::is_signed_v<param_value_t>)
                {
                    try
                    {
                        param_value_to_store = std::stoll(path_parameter);
                    }
                    // Path parameter string value representation is not a number
                    catch (const std::exception&)
                    {
                        return false;
                    }
                }
            }
            else
            {
                return false;
            }

            return true;
        }

        // Assign the query parameter to the given param_value_to_store, converting it to variable type
        // Return true on successful conversion and assignment, otherwise return false
        template <typename param_value_t>
        bool get_query_parameter(
            std::string_view uri, 
            std::string_view param_name, 
            param_value_t& param_value_to_store) 
        {
            size_t start_position{0}, equal_sign_position, end_position;

            // Query parameter is a string
            if constexpr (std::is_same_v<param_value_t, std::string>)
            {
                start_position = uri.find(param_name);

                // No query parameter found
                if (start_position == std::string::npos)
                {
                    return false;
                }

                equal_sign_position = start_position + param_name.size();

                // No equal sign so it is invalid format
                if (equal_sign_position >= uri.size() || uri[equal_sign_position] != '=')
                {
                    return false;
                }

                end_position = uri.find('&', equal_sign_position + 1);

                param_value_to_store = uri.substr(equal_sign_position + 1, end_position - equal_sign_position - 1);
            }
            // Query parameter is a number
            else if constexpr (std::is_integral_v<param_value_t>)
            {
                start_position = uri.find(param_name);

                // No query parameter found
                if (start_position == std::string::npos)
                {
                    return false;
                }

                equal_sign_position = start_position + param_name.size();

                // No equal sign so it is invalid format
                if (equal_sign_position >= uri.size() || uri[equal_sign_position] != '=')
                {
                    return false;
                }

                end_position = uri.find('&', equal_sign_position + 1);

                // Unsigned integers are got from string to uint64_t
                if constexpr (std::is_unsigned_v<param_value_t>)
                {
                    try
                    {
                        param_value_to_store = std::stoull(
                                std::string{uri.substr(equal_sign_position + 1, end_position - equal_sign_position - 1)},
                                &start_position);

                        // Query parameter string value representation contains non digits after valid number
                        if (start_position != 
                            uri.substr(equal_sign_position + 1, end_position - equal_sign_position - 1).size())
                        {
                            return false;
                        }
                    }
                    // Query parameter string value representation is not a number
                    catch (const std::exception&)
                    {
                        return false;
                    }
                }
                // Signed integers are got from string to int64_t
                else if constexpr (std::is_signed_v<param_value_t>)
                {
                    try
                    {
                        param_value_to_store = std::stoll(
                            std::string{uri.substr(equal_sign_position + 1, end_position - equal_sign_position - 1)},
                            &start_position);

                        // Query parameter string value representation contains non digits after valid number
                        if (start_position != 
                            uri.substr(equal_sign_position + 1, end_position - equal_sign_position - 1).size())
                        {
                            return false;
                        }
                    }
                    // Query parameter string value representation is not a number
                    catch (const std::exception&)
                    {
                        return false;
                    }
                }
            }
            // Query parameter is an array(process only std::vector<>)
            else if constexpr (std::is_same_v<
                param_value_t, std::vector<typename param_value_t::value_type, typename param_value_t::allocator_type>>)
            {
                param_value_to_store.clear();

                do
                {
                    start_position = uri.find(param_name, start_position);

                    // No query parameter found
                    if (start_position == std::string::npos)
                    {
                        // There have to be some array query parameters with given name
                        if (param_value_to_store.empty())
                        {
                            return false;
                        }
                        else
                        {
                            break;
                        }
                    }

                    equal_sign_position = start_position + param_name.size() + 2;

                    // No equal sign so it is invalid format
                    if (equal_sign_position >= uri.size() || uri[equal_sign_position] != '=')
                    {
                        return false;
                    }

                    // Array parameter name has to be followed by []
                    if (uri.substr(equal_sign_position - 2, 2) != "[]")
                    {
                        return false;
                    }

                    end_position = start_position = uri.find('&', equal_sign_position + 1);

                    // Array elements type is a string
                    if constexpr (std::is_same_v<typename param_value_t::value_type, std::string>)
                    {
                        param_value_to_store.emplace_back(
                            uri.substr(equal_sign_position + 1, end_position - equal_sign_position - 1));
                    }
                    //Array elements type is a number
                    else if constexpr (std::is_integral_v<typename param_value_t::value_type>)
                    {
                        // Unsigned integers are got from string to uint64_t
                        if constexpr (std::is_unsigned_v<typename param_value_t::value_type>)
                        {
                            try
                            {
                                param_value_to_store.emplace_back(std::stoull(std::string{uri.substr(
                                    equal_sign_position + 1, end_position - equal_sign_position - 1)}));
                            }
                            // Query parameter string value representation is not a number
                            catch (const std::exception&)
                            {
                                return false;
                            }
                        }
                        // Signed integers are got from string to int64_t
                        else if constexpr (std::is_signed_v<typename param_value_t::value_type>)
                        {
                            try
                            {
                                param_value_to_store.emplace_back(std::stoll(std::string{uri.substr(
                                    equal_sign_position + 1, end_position - equal_sign_position - 1)}));
                            }
                            // Query parameter string value representation is not a number
                            catch (const std::exception&)
                            {
                                return false;
                            }
                        }
                    }
                } while (end_position != std::string::npos);  
            }
            else
            {
                return false;
            }

            return true;
        }
    }
}

#endif