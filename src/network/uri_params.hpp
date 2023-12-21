#ifndef URI_PARAMS_HPP
#define URI_PARAMS_HPP

#include <string_view>
#include <boost/json/object.hpp>

namespace json = boost::json;

namespace uri_params
{
    enum class type
    {
        NO, 
        PATH,
        QUERY,
        PATH_AND_QUERY
    };

    // Return the uri excluding query or path parameters
    std::string_view get_unparameterized_uri(std::string_view uri);

    type determine_uri_parameters_type(std::string_view uri);

    // Match the path parameter to the given param_value_to_store, converting it to this variable type
    // Return true on successful conversion and assignment otherwise return false
    template <typename param_value_t>
    bool get_path_parameter(
        std::string_view uri, 
        param_value_t& param_value_to_store)
    {
        // Start with position 5 because the uri has to start with /api/... and no need to capture these slashes
        size_t slash_position = 5;

        // Skip the next slash as it is the part of the uri
        slash_position = uri.find('/', 5);

        // If we didn't find the 4th slash in the uri then there is no path parameter
        if ((slash_position = uri.find('/', slash_position + 1)) == std::string::npos)
        {
            return false;
        }

        // If we found the next slash then there are too many slashes for our uri
        if (uri.find('/', slash_position + 1) != std::string::npos)
        {
            return false;
        }

        // Path parameter is a string
        if constexpr (std::is_same_v<param_value_t, std::string>)
        {
            param_value_to_store = uri.substr(slash_position + 1);
        }
        // Path parameter is a number
        else if constexpr (std::is_integral_v<param_value_t>)
        {
            // Unsigned integers are got from string to uint64_t
            if constexpr (std::is_unsigned_v<param_value_t>)
            {
                try
                {
                    param_value_to_store = std::stoull(std::string{uri.substr(slash_position + 1)});
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
                    param_value_to_store = std::stoll(std::string{uri.substr(slash_position + 1)});
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

    json::object get_query_parameters(std::string_view uri, size_t expected_params_number = 1);

    // Match each query parameter by its name to the corresponding variable, converting it to this variable type
    // Return true on all successful conversions and assignments otherwise return false
    template <typename param_value_t, typename... params_t>
    bool get_query_parameters(
        std::string_view uri, 
        std::string_view param_name, 
        param_value_t& param_value_to_store, 
        params_t&&... other_params) 
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
                        std::string{uri.substr(equal_sign_position + 1, end_position - equal_sign_position - 1)});

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

        // Call the same function if there are query parameters left to process
        if constexpr (sizeof...(other_params) > 0) 
        {
            if (!get_query_parameters(uri, std::forward<params_t>(other_params)...))
            {
                return false;
            }
        }

        return true;
    }
}

#endif