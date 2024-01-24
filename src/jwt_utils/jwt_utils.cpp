#include <jwt_utils/jwt_utils.hpp>

std::pair<std::string, std::string> jwt_utils::create_tokens(const json::object& custom_claims)
{
    std::pair<std::string, std::string> tokens;
    const auto current_time = jwt::date::clock::now();

    // Create and initialize access token
    auto builder = jwt::create<traits>().set_expires_at(current_time + config::ACCESS_TOKEN_EXPIRY_TIME_MINUTES);

    for (const auto& [key, value] : custom_claims)
        builder = builder.set_payload_claim(key, value);
    
    // Set timestamp of creation time to enable creating different tokens with the same claims within one second 
    builder.set_payload_claim("timestamp", std::chrono::high_resolution_clock::now().time_since_epoch().count());

    tokens.first = builder.sign(_crypto_algrorithm);

    // Create and initialize refresh token
    builder = jwt::create<traits>().set_expires_at(current_time + config::REFRESH_TOKEN_EXPIRY_TIME_DAYS);

    for (const auto& [key, value] : custom_claims)
        builder = builder.set_payload_claim(key, value);
    
    // Set timestamp of creation time to enable creating different tokens with the same claims within one second 
    builder.set_payload_claim("timestamp", std::chrono::high_resolution_clock::now().time_since_epoch().count());

    tokens.second = builder.sign(_crypto_algrorithm);

    return tokens;
}

std::pair<std::string, std::string> jwt_utils::refresh_tokens(const std::string& refresh_token)
{
    std::pair<std::string, std::string> tokens;
    const auto current_time = jwt::date::clock::now();
    const auto refresh_token_payload = jwt::decode<traits>(refresh_token).get_payload_json();

    // Create and initialize access token
    auto builder = jwt::create<traits>();

    for (const auto& [key, value] : refresh_token_payload)
        builder = builder.set_payload_claim(key, value);
    
    // Set timestamp of creation time to enable creating different tokens with the same claims within one second 
    builder.set_payload_claim("timestamp", std::chrono::high_resolution_clock::now().time_since_epoch().count());
    
    tokens.first = builder
        .set_expires_at(current_time + config::ACCESS_TOKEN_EXPIRY_TIME_MINUTES)
        .sign(_crypto_algrorithm);

    // Create and initialize refresh token
    builder = jwt::create<traits>();

    for (const auto& [key, value] : refresh_token_payload)
        builder = builder.set_payload_claim(key, value);
    
    // Set timestamp of creation time to enable creating different tokens with the same claims within one second 
    builder.set_payload_claim("timestamp", std::chrono::high_resolution_clock::now().time_since_epoch().count());
    
    tokens.second = builder
        .set_expires_at(current_time + config::REFRESH_TOKEN_EXPIRY_TIME_DAYS)
        .sign(_crypto_algrorithm);

    return tokens;
}

json::object jwt_utils::get_token_payload(const std::string& token)
{
    return jwt::decode<traits>(token).get_payload_json();
}

bool jwt_utils::is_token_valid(const std::string &token)
{
    try
    {
        _verifier.verify(jwt::decode<traits>(token));
    }
    catch (const std::exception&)
    {
        return false;
    }
    
    return true;
}