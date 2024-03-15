#include <request_handlers/user/user_request_handlers.hpp>

void request_handlers::user::login(const request_params& request, response_params& response)
{
    try
    {
        json::object body_json = json::parse(request.body).as_object();

        auto db_conn = database_connections_pool::get<user_database_connection>();
 
        // No available connections
        if (!db_conn)
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "No available database connections");
        }

        // Try to login the user by its credentials
        std::optional<std::size_t> user_id_opt = db_conn->login(
            body_json.at("nickname").as_string(), 
            body_json.at("password").as_string());

        // An error occured with database connection
        if (!user_id_opt.has_value())
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "Internal server error occured");
        }

        // User's credentials are invalid
        if (user_id_opt.value() == 0)
        {
            return prepare_error_response(
                response, 
                http::status::unauthorized, 
                "Invalid login credentials");
        }

        // Create a pair of access and refresh token respectively
        std::pair<std::string, std::string> tokens = jwt_utils::create_tokens(
            json::object
            {
                {"userId", user_id_opt.value()}, 
                {"nickname", body_json.at("nickname").as_string()},
                {"timestamp", std::chrono::high_resolution_clock::now().time_since_epoch().count()}
            });
        
        // Create new session
        if (!db_conn->insert_session(
            user_id_opt.value(),
            tokens.second,
            request.user_agent,
            request.user_ip,
            !body_json.at("rememberMe").as_bool()))
        // An error occured with database connection
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "Internal server error occured");
        }

        // Initialize response
        response.refresh_token = tokens.second;
        response.remember_me = body_json.at("rememberMe").as_bool();

        // If remember me flag is true then max age of cookies is corresponding refresh token expiry time
        // otherwise set it to -1 to signal that cookies have to be erased after browser closure
        if (response.remember_me)
        {
            response.max_age = config::REFRESH_TOKEN_EXPIRY_TIME_DAYS;
        }
        else
        {
            response.max_age = std::chrono::seconds{-1};
        }

        response.body = json::serialize(
            json::object
            {
                {"nickname", body_json.at("nickname").as_string()},
                {"accessToken", tokens.first}
            });
    }
    catch (const std::exception&)
    {
        return prepare_error_response(
            response, 
            http::status::unprocessable_entity, 
            "Invalid body format");
    }
}

void request_handlers::user::logout(const request_params& request, response_params& response)
{
    auto db_conn = database_connections_pool::get<user_database_connection>();

    // No available connections
    if (!db_conn)
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "No available database connections");
    }

    // Close current session by refresh token and update session's data
    std::optional<bool> refresh_token_is_found_opt = 
        db_conn->close_current_session(request.refresh_token, request.user_ip); 
        
    // An error occured with database connection
    if (!refresh_token_is_found_opt.has_value())
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "Internal server error occured");
    }

    // Refresh token was not found in database
    if (!refresh_token_is_found_opt.value())
    {
        return prepare_error_response(
            response, 
            http::status::unauthorized, 
            "Invalid refresh token");
    }

    // We need cookies to be deleted in browser after receiving this response so set max age to 0
    // and just erase refresh token by assigning it 'deleted' value to signify the deletion intention
    response.refresh_token = "deleted";
    response.remember_me = false;
    response.max_age = std::chrono::seconds{0};
}

void request_handlers::user::refresh_tokens(const request_params& request, response_params& response)
{
    // Create a new pair of access and refresh token respectively with the same token claims
    std::pair<std::string, std::string> tokens = jwt_utils::refresh_tokens(std::string{request.refresh_token});

    auto db_conn = database_connections_pool::get<user_database_connection>();

    // No available connections
    if (!db_conn)
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "No available database connections");
    }

    // Update refresh token and session's data
    std::optional<bool> refresh_token_is_found_opt = 
        db_conn->update_refresh_token(
            request.refresh_token, 
            tokens.second,
            request.user_ip);

    // An error occured with database connection
    if (!refresh_token_is_found_opt.has_value())
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "Internal server error occured");
    }

    // Refresh token was not found in database
    if (!refresh_token_is_found_opt.value())
    {
        return prepare_error_response(
            response, 
            http::status::unauthorized, 
            "Invalid refresh token");
    }

    // Initialize response
    response.refresh_token = tokens.second;
    response.remember_me = request.remember_me;

    // If remember me flag is true then max age of cookies is corresponding refresh token expiry time
    // otherwise set it to -1 to signal that cookies have to be erased after browser closure
    if (response.remember_me)
    {
        response.max_age = config::REFRESH_TOKEN_EXPIRY_TIME_DAYS;
    }
    else
    {
        response.max_age = std::chrono::seconds{-1};
    }

    std::string nickname;

    // Get nickname claim from the refresh token
    jwt_utils::get_token_claim(tokens.second, "nickname", nickname);

    response.body = json::serialize(
        json::object
        {
            {"nickname", nickname},
            {"accessToken", tokens.first}
        }
    );
}

void request_handlers::user::get_sessions_info(const request_params& request, response_params& response)
{
    auto db_conn = database_connections_pool::get<user_database_connection>();

    // No available connections
    if (!db_conn)
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "No available database connections");
    }

    size_t user_id;

    // Get user id claim from the refresh token
    jwt_utils::get_token_claim(std::string{request.refresh_token}, "userId", user_id);
    
    // Get all sessions info in json format
    std::optional<json::object> sessions_json_opt = db_conn->get_sessions_info(
        user_id,
        request.refresh_token); 

    // An error occured with database connection
    if (!sessions_json_opt.has_value())
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "Internal server error occured");
    }
    
    // Refresh token was not found in database
    if (sessions_json_opt->empty())
    {
        return prepare_error_response(
            response, 
            http::status::unauthorized, 
            "Invalid refresh token");
    }

    // Initialize body with string representation of json
    response.body = json::serialize(sessions_json_opt.value());
}

void request_handlers::user::close_session(const request_params& request, response_params& response)
{
    size_t session_id;

    if (!http_utils::uri::get_path_parameter(request.uri, request.uri_template, "sessionId", session_id))
    {
        return prepare_error_response(
            response, 
            http::status::unprocessable_entity, 
            "Invalid session id");
    }

    auto db_conn = database_connections_pool::get<user_database_connection>();

    // No available connections
    if (!db_conn)
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "No available database connections");
    }

    size_t user_id;

    // Get user id claim from the access token
    jwt_utils::get_token_claim(std::string{request.access_token}, "userId", user_id);
    
    // Close session with specified session id
    std::optional<bool> is_session_closed_opt = db_conn->close_own_session(session_id, user_id); 

    // An error occured with database connection
    if (!is_session_closed_opt.has_value())
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "Internal server error occured");
    }

    // Either session with session_id was not found or it doesn't belong to the user with user_id
    if (!is_session_closed_opt.value())
    {
        return prepare_error_response(
            response, 
            http::status::unprocessable_entity, 
            "Invalid session id");
    }
}

void request_handlers::user::close_all_sessions_except_current(const request_params& request, response_params& response)
{
    auto db_conn = database_connections_pool::get<user_database_connection>();

    // No available connections
    if (!db_conn)
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "No available database connections");
    }

    size_t user_id;

    // Get user id claim from the refresh token
    jwt_utils::get_token_claim(std::string{request.refresh_token}, "userId", user_id);
    
    // Close all user's sessions except current one
    std::optional<bool> are_sessions_closed_opt = 
        db_conn->close_all_sessions_except_current(user_id, request.refresh_token); 

    // An error occured with database connection
    if (!are_sessions_closed_opt.has_value())
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "Internal server error occured");
    }

    // Refresh token was not found in database
    if (!are_sessions_closed_opt.value())
    {
        return prepare_error_response(
            response, 
            http::status::unauthorized, 
            "Invalid refresh token");
    }
}

void request_handlers::user::change_password(const request_params& request, response_params& response)
{
    static const boost::regex password_validation{R"((?=.*[0-9])(?=.*[a-z])(?=.*[A-Z])(?=.*[,\/|@#$_&\-+()\[\]{}*"'`~=:;!?])[0-9a-zA-Z.,\/|@#$_&\-+()\[\]{}*"'`~=:;!?]{8,32})"};

    try
    {
        json::object body_json = json::parse(request.body).as_object();

        // The same password is forbidden
        if (body_json.at("currentPassword").as_string() == body_json.at("newPassword").as_string())
        {
            return prepare_error_response(
                response, 
                http::status::conflict, 
                "New password must be different from the current one");
        }

        // New password doesn't satisfy the format requirements
        if (!boost::regex_match(body_json.at("newPassword").as_string().c_str(), password_validation))
        {
            return prepare_error_response(
                response, 
                http::status::unprocessable_entity, 
                "Invalid new password format");
        }

        auto db_conn = database_connections_pool::get<user_database_connection>();

        // No available connections
        if (!db_conn)
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "No available database connections");
        }

        size_t user_id;

        // Get user id claim from the refresh token
        jwt_utils::get_token_claim(std::string{request.refresh_token}, "userId", user_id);

        // Check if the current password is valid
        std::optional<bool> is_current_password_valid_opt = 
            db_conn->validate_password(
                user_id, 
                body_json.at("currentPassword").as_string());

        // An error occured with database connection
        if (!is_current_password_valid_opt.has_value())
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "Internal server error occured");
        }

        // Current password is not valid
        if (!is_current_password_valid_opt.value())
        {
            return prepare_error_response(
                response, 
                http::status::unprocessable_entity, 
                "Invalid current password");
        }

        std::optional<bool> are_sessions_closed_opt = db_conn->change_password(
            user_id, 
            body_json.at("newPassword").as_string(),
            request.refresh_token);

        // An error occured with database connection
        if (!are_sessions_closed_opt.has_value())
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "Internal server error occured");
        }

        // Refresh token was not found in database
        if (!are_sessions_closed_opt.value())
        {
            return prepare_error_response(
                response, 
                http::status::unauthorized, 
                "Invalid refresh token");
        }
    }
    catch (const std::exception&)
    {
        return prepare_error_response(
            response, 
            http::status::unprocessable_entity, 
            "Invalid body format");
    }  
}