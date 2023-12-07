#include <request_handlers/request_handlers.hpp>
#include <network/http_session.hpp>

void request_handlers::prepare_error_response(
    http::response<http::string_body>& response, 
    const http::status response_status, 
    const std::string_view& error_message)
{
    // Clear unnecessary field
    response.erase(http::field::set_cookie);
    
    response.result(response_status);
    response.body().assign(R"({"error":")").append(error_message).append(R"("})");
    response.prepare_payload();
}

void request_handlers::handle_login(
    const http::request_parser<http::string_body>& request_parser, 
    http::response<http::string_body>& response)
{
    json::value body_json;
    try
    {
        body_json = json::parse(request_parser.get().body());

        auto db = database_pool::get();

        // No available connections
        if (!db)
        {
            prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "No available database connections");
            return;
        }

        auto user_id = db->login(body_json.at("nickname").as_string(), body_json.at("password").as_string());
        database_pool::release(std::move(db));

        // An error occured with database connection
        if (!user_id)
        {
            prepare_error_response(response, http::status::internal_server_error, "Internal server error occured");
            return;
        }

        // User's credentials are invalid
        if (*user_id == 0)
        {
            prepare_error_response(response, http::status::unauthorized, "Invalid login credentials");
            return;
        }

        std::pair<std::string, std::string> tokens{jwt_utils::create_tokens(
            json::object
            {
                {"userId", *user_id}, 
                {"nickname", body_json.at("nickname").as_string()},
                // {"refreshTokenId", db.get_user_token_id(user_id)}
                // {"refreshTokenId", 987654321}
            })};
        // ...
        response.erase(http::field::set_cookie);
        response.result(http::status::ok);
        response.insert(
            http::field::set_cookie, 
            cookie_utils::set_cookie(
                "refreshToken", 
                tokens.second, 
                config::REFRESH_TOKEN_EXPIRY_TIME));
        response.insert(
            http::field::set_cookie,
            cookie_utils::set_cookie(
                "rememberMe", 
                body_json.at("rememberMe").as_bool() ? "true" : "false", 
                config::REFRESH_TOKEN_EXPIRY_TIME));
        response.body() = json::serialize(json::object
        {
            {"nickname", body_json.at("nickname").as_string()},
            {"accessToken", tokens.first}
        });
        response.prepare_payload();
    }
    catch(const std::exception&)
    {
        prepare_error_response(response, http::status::unprocessable_entity, "Incorrect request format");
        return;
    }
}

void request_handlers::handle_logout(
    const http::request_parser<http::string_body>& request_parser, 
    http::response<http::string_body>& response)
{
    prepare_error_response(response, http::status::ok, "test");
}

void request_handlers::handle_refresh(
    const http::request_parser<http::string_body>& request_parser, 
    http::response<http::string_body>& response)
{
    
}

void request_handlers::handle_sessions_info(
    const http::request_parser<http::string_body>& request_parser, 
    http::response<http::string_body>& response)
{
    
}

void request_handlers::process_downloaded_files(std::list<std::pair<size_t, std::string>>&& file_ids_and_paths)
{
}
