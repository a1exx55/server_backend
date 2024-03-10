#ifndef ERROR_RESPONSE_PREPARING_HPP
#define ERROR_RESPONSE_PREPARING_HPP

#include <network/request_and_response_params.hpp>

namespace request_handlers
{
    inline void prepare_error_response(
        response_params& response,
        boost::beast::http::status status,
        std::string_view error_message)
    {
        response.status = status;
        response.body = json::serialize(
            json::object
            {
                {"error", error_message}
            });
    }
}

#endif