#ifndef JWT_UTILS_HPP
#define JWT_UTILS_HPP

//local
#include <config.hpp>

//external
#include <jwt-cpp/traits/boost-json/traits.h>

using traits = jwt::traits::boost_json;
namespace json = boost::json;

class jwt_utils
{ 
    public:
        // Use custom claims to construct tokens payload(the same payload in both except expiry time)
        // Return a pair of access and refresh tokens respectively
        static std::pair<std::string, std::string> create_tokens(const json::object& custom_claims);
        
        // Use refresh token to get its custom claims and construct tokens payload(the same payload in both except expiry time)
        // Return a pair of access and refresh tokens respectively
        static std::pair<std::string, std::string> refresh_tokens(const std::string& refresh_token);

        // Return token payload as json object
        static json::object get_token_payload(const std::string& token);

        // Check if token is valid
        static bool is_token_valid(const std::string &token);

    private:
        inline static const jwt::algorithm::hs256 _crypto_algrorithm{config::JWT_SECRET_KEY};

        inline static const jwt::verifier<jwt::default_clock, traits> _verifier{jwt::verify<traits>()
		    .allow_algorithm(_crypto_algrorithm)};

};

#endif