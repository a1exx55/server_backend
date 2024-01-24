#include <network/uri_params.hpp>

std::string_view uri_params::get_unparameterized_uri(std::string_view uri)
{
    // Start with position 5 because the uri has to start with /api/... and no need to capture these slashes
    size_t previous_delimiter_position = 5, delimiter_position;

    // Skip the next slash as it is the part of the uri
    if ((delimiter_position = uri.find('/', previous_delimiter_position)) == std::string::npos)
    {
        return {};
    }

    previous_delimiter_position = delimiter_position + 1;

    // If we found the 4th slash in the uri then the path parameter start
    // If we found the question mark in the uri then the query parameters start
    if ((delimiter_position = uri.find('/', previous_delimiter_position)) != std::string::npos ||
        (delimiter_position = uri.find('?', previous_delimiter_position)) != std::string::npos)
    {
        return uri.substr(0, delimiter_position);
    }

    // The uri is unparameterized
    return uri;
}

uri_params::type uri_params::determine_uri_parameters_type(std::string_view uri)
{
    // Start with position 5 because the uri has to start with /api/... and no need to capture these slashes
    size_t previous_delimiter_position = 5, delimiter_position;

    // Skip the next slash as it is the part of the uri
    // If slash is absent then uri is invalid
    if ((delimiter_position = uri.find('/', previous_delimiter_position)) == std::string::npos)
    {
        return uri_params::type::no;
    }

    previous_delimiter_position = delimiter_position + 1;

    // If we found the 4th slash in the uri then there is path parameter
    if ((delimiter_position = uri.find('/', previous_delimiter_position)) != std::string::npos)
    {
        // If we also found the question mark in the uri then the there are both path and query parameters
        if (uri.find('?', delimiter_position + 1) != std::string::npos)
        {
            return uri_params::type::path_and_query;
        }

        // If we didn't find the question mark in the uri then there is only path parameter
        return uri_params::type::path;
    }

    // If we found the question mark in the uri then there are only query parameters
    if (uri.find('?', previous_delimiter_position) != std::string::npos)
    {
        return uri_params::type::query;
    }

    // The uri is unparameterized
    return uri_params::type::no;
}

json::object uri_params::get_query_parameters(std::string_view uri, size_t expected_params_number)
{
    // Start with position 5 because the uri has to start with /api/... and no need to capture these slashes
    size_t delimiter_position = 5;

    // Skip the next slash as it is the part of the uri
    delimiter_position = uri.find('/', 5);

    // If we found the 4th slash in the uri then there is path parameter instead of query ones
    if (uri.find('/', delimiter_position + 1) != std::string::npos)
    {
        return {};
    }

    // Query parameters must start with question mark
    if ((delimiter_position = uri.find('?', delimiter_position + 1)) == std::string::npos)
    {
        return {};
    }

    size_t start_position = delimiter_position + 1, end_position;
    json::object query_parameters;
    
    try
    {
        // Reserve the expected number of query parameters to avoid reallocating memory 
        query_parameters.reserve(expected_params_number);

        do
        {
            delimiter_position = uri.find('=', start_position);

            // No equals sign so it is invalid format
            if (delimiter_position == std::string::npos)
            {
                return {};
            }

            end_position = uri.find('&', delimiter_position + 1);

            // If the key ends with [] then it is the array parameter
            // otherwise it is just the regular string parameter
            if (uri.substr(delimiter_position - 2, 2) == "[]")
            {
                // If there are array elements with the current key then just emplace the current value back
                // otherwise emplace the array with the current value to the current key
                if (query_parameters.contains(uri.substr(start_position, delimiter_position - start_position - 2)))
                {
                    query_parameters[uri.substr(start_position, delimiter_position - start_position - 2)]
                        .as_array().emplace_back(uri.substr(
                            delimiter_position + 1, 
                            end_position - delimiter_position - 1));
                }
                else
                {
                    query_parameters.emplace(
                        uri.substr(start_position, delimiter_position - start_position - 2), 
                        json::array{uri.substr(delimiter_position + 1, end_position - delimiter_position - 1)});
                }
            }
            else
            {
                query_parameters.emplace(
                    uri.substr(start_position, delimiter_position - start_position),
                    uri.substr(delimiter_position + 1, end_position - delimiter_position - 1));
            }

            start_position = end_position + 1;
        } 
        while (end_position != std::string::npos);
    }
    catch (const std::exception&)
    {
        return {};
    }

    return query_parameters;
}