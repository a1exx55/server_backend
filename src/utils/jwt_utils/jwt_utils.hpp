#ifndef JWT_UTILS_HPP
#define JWT_UTILS_HPP

//local
#include <config.hpp>

//external
#include <jwt-cpp/traits/boost-json/traits.h>

using traits = jwt::traits::boost_json;
namespace json = boost::json;

enum class jwt_token_type
{
    no = 0,
    access_token,
    refresh_token
};

class jwt_utils
{ 
    public:
        // Use custom claims to construct tokens payload(the same payload in both except expiry time)
        // Return a pair of access and refresh tokens respectively
        static std::pair<std::string, std::string> create_tokens(const json::object& custom_claims);
        
        // Use refresh token to get its custom claims and construct tokens payload
        // (the same payload in both except expiry time)
        // Return a pair of access and refresh tokens respectively
        static std::pair<std::string, std::string> refresh_tokens(const std::string& refresh_token);

        // Return token payload as json object
        static json::object get_token_payload(const std::string& token);

        // Get token claim by its name and assign it to the claim_value_to_store variable, 
        // converting claim to the variable type
        // Return true on successfull conversion and assignment otherwise return false
        template <typename param_value_t>
        static bool get_token_claim(
            const std::string& token, 
            std::string_view claim_name,
            param_value_t& claim_value_to_store)
        {
            // Token claim is a string
            if constexpr (std::is_same_v<param_value_t, std::string>)
            {
                try
                {
                    claim_value_to_store = jwt::decode<traits>(token).get_payload_json().at(claim_name).as_string().data();
                }
                // Either claim_name is not found in json or it is not a string 
                catch(const std::exception&)
                {
                    return false;
                }   
            }
            // Token claim is a number
            else if constexpr (std::is_integral_v<param_value_t>)
            {
                // Unsigned integers are got from string to uint64_t
                if constexpr (std::is_unsigned_v<param_value_t>)
                {
                    try
                    {
                        claim_value_to_store = jwt::decode<traits>(token).get_payload_json()
                            .at(claim_name).to_number<param_value_t>();
                    }
                    // Either claim_name is not found in json or it is not a unsigned number 
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
                        claim_value_to_store = jwt::decode<traits>(token).get_payload_json()
                            .at(claim_name).to_number<param_value_t>();
                    }
                    // Either claim_name is not found in json or it is not a signed number 
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
        // Check if token is valid
        static bool is_token_valid(const std::string &token);

    private:
        inline static const jwt::algorithm::hs256 _crypto_algrorithm{config::jwt_secret_key};

        inline static const jwt::verifier<jwt::default_clock, traits> _verifier{jwt::verify<traits>()
		    .allow_algorithm(_crypto_algrorithm)};

};

#endif