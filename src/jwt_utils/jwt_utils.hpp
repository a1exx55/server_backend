#ifndef JWT_UTILS_HPP
#define JWT_UTILS_HPP

//local
#include <jwt-cpp/traits/boost-json/traits.h>
#include <config.hpp>

using traits = jwt::traits::boost_json;
namespace json = boost::json;

class jwt_utils
{ 
    public:
        // Use custom claims to construct tokens payload
        // Return a pair of access and refresh tokens respectively
        std::pair<std::string, std::string> create_tokens(const json::value& custom_claims);

        // Use refresh token to get its custom claims and construct tokens payload
        // Return a pair of access and refresh tokens respectively
        std::pair<std::string, std::string> refresh_tokens(const std::string& refresh_token);

        // Return token payload as json object
        json::value get_token_payload(const std::string& token);

        // Check if token is valid
        bool is_token_valid(const std::string &token);

    private:
        jwt::algorithm::hs256 _crypto_algrorithm{config::JWT_SECRET_KEY};

        jwt::verifier<jwt::default_clock, traits> _verifier = jwt::verify<traits>()
		    .allow_algorithm(_crypto_algrorithm);

};

#endif