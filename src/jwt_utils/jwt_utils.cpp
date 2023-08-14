#include <jwt_utils/jwt_utils.hpp>

std::pair<std::string, std::string> jwt_utils::create_tokens(const json::value& custom_claims)
{
    std::pair<std::string, std::string> tokens;
    const auto current_time = jwt::date::clock::now();

    // Create and initialize access token
    auto builder = jwt::create<traits>().set_expires_at(current_time + config::ACCESS_TOKEN_EXPIRY_TIME_MIN);

    for (const auto& [key, value] : custom_claims.as_object())
        builder = builder.set_payload_claim(key, value);
    
    tokens.first = builder.sign(_crypto_algrorithm);

    // Create and initialize refresh token
    builder = jwt::create<traits>().set_expires_at(current_time + config::REFRESH_TOKEN_EXPIRY_TIME_DAYS);

    for (const auto& [key, value] : custom_claims.as_object())
        builder = builder.set_payload_claim(key, value);
    
    tokens.second = builder.sign(_crypto_algrorithm);

    return tokens;
}

std::pair<std::string, std::string> jwt_utils::refresh_tokens(const std::string& refresh_token)
{
    std::pair<std::string, std::string> tokens;
    const auto current_time = jwt::date::clock::now();
    const auto decoded_refresh_token = jwt::decode<traits>(refresh_token);

    // Create and initialize access token
    auto builder = jwt::create<traits>();

    for (const auto& [key, value] : decoded_refresh_token.get_payload_json())
        builder = builder.set_payload_claim(key, value);
    
    tokens.first = builder
        .set_expires_at(current_time + config::ACCESS_TOKEN_EXPIRY_TIME_MIN)
        .sign(_crypto_algrorithm);

    // Create and initialize refresh token
    builder = jwt::create<traits>();

    for (const auto& [key, value] : decoded_refresh_token.get_payload_json())
        builder = builder.set_payload_claim(key, value);
    
    tokens.second = builder
        .set_expires_at(current_time + config::REFRESH_TOKEN_EXPIRY_TIME_DAYS)
        .sign(_crypto_algrorithm);

    return tokens;
}

json::value jwt_utils::get_token_payload(const std::string& token)
{
    return jwt::decode<traits>(token).get_payload_json();
}

bool jwt_utils::is_token_valid(const std::string &token)
{
    try
    {
        _verifier.verify(jwt::decode<traits>(token));
    }
    catch (const jwt::error::token_verification_exception&)
    {
        return false;
    }
    
    return true;
}