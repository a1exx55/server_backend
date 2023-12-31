#include <request_handlers/request_handlers.hpp>
#include <network/http_session.hpp>

void request_handlers::prepare_error_response(
    response_params& response, 
    const http::status error_status, 
    const std::string_view& error_message)
{
    response.error_status = error_status;
    response.body.assign(R"({"error":")").append(error_message).append(R"("})");
}

void request_handlers::login(const request_params& request, response_params& response)
{
    try
    {
        json::object body_json = json::parse(request.body).as_object();

        database_connection_wrapper db_conn = database_connections_pool::get();
 
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
            response.max_age = config::REFRESH_TOKEN_EXPIRY_TIME;
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

void request_handlers::logout(const request_params& request, response_params& response)
{
    database_connection_wrapper db_conn = database_connections_pool::get();

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

void request_handlers::refresh_tokens(const request_params& request, response_params& response)
{
    // Create a new pair of access and refresh token respectively with the same token claims
    std::pair<std::string, std::string> tokens = jwt_utils::refresh_tokens(std::string{request.refresh_token});

    database_connection_wrapper db_conn = database_connections_pool::get();

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
        response.max_age = config::REFRESH_TOKEN_EXPIRY_TIME;
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

void request_handlers::get_sessions_info(const request_params& request, response_params& response)
{
    database_connection_wrapper db_conn = database_connections_pool::get();

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

void request_handlers::close_session(const request_params& request, response_params& response)
{
    size_t session_id;

    if (!uri_params::get_path_parameter(request.uri, session_id))
    {
        return prepare_error_response(
            response, 
            http::status::unprocessable_entity, 
            "Invalid session id");
    }

    database_connection_wrapper db_conn = database_connections_pool::get();

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

void request_handlers::close_all_sessions_except_current(const request_params& request, response_params& response)
{
    database_connection_wrapper db_conn = database_connections_pool::get();

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

void request_handlers::change_password(const request_params& request, response_params& response)
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

        database_connection_wrapper db_conn = database_connections_pool::get();

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

void request_handlers::get_folders_info(const request_params& request, response_params& response)
{
    database_connection_wrapper db_conn = database_connections_pool::get();

    // No available connections
    if (!db_conn)
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "No available database connections");
    }
    
    // Get all folders info in json format
    std::optional<json::array> folders_info_array_opt = db_conn->get_folders_info(); 

    // An error occured with database connection
    if (!folders_info_array_opt.has_value())
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "Internal server error occured");
    }
    
    // Initialize body with string representation of json
    response.body = json::serialize(folders_info_array_opt.value());
}

void request_handlers::create_folder(const request_params& request, response_params& response)
{
    try
    {
        std::string folder_name = json::parse(request.body).as_object().at("folderName").as_string().c_str();

        // Trim whitespaces at the beggining and at the end of the folder name
        boost::algorithm::trim(folder_name);

        // Transform the string to UnicodeString to check the actual length of unicode symbols
        icu::UnicodeString folder_name_unicode{folder_name.c_str()}; 

        // Folder name can't be less than 3 and longer than 64 symbols
        if (folder_name_unicode.length() < 3 || folder_name_unicode.length() > 64) 
        {
            return prepare_error_response(
                response, 
                http::status::unprocessable_entity, 
                "Invalid folder name format");
        }

        database_connection_wrapper db_conn = database_connections_pool::get();
 
        // No available connections
        if (!db_conn)
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "No available database connections");
        }

        std::optional<bool> does_folder_already_exist = db_conn->check_folder_existence_by_name(folder_name);

        // An error occured with database connection
        if (!does_folder_already_exist.has_value())
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "Internal server error occured");
        }

        // Folder with given name already exists
        if (does_folder_already_exist.value())
        {
            return prepare_error_response(
                response, 
                http::status::conflict, 
                "Folder with this name already exists");
        }

        size_t user_id;
        std::string user_name;

        // Get user's id and name claims from the access token
        jwt_utils::get_token_claim(std::string{request.access_token}, "userId", user_id);
        jwt_utils::get_token_claim(std::string{request.access_token}, "nickname", user_name);

        // Insert folder with given name and get a pair of json with data of this folder and 
        // the folder path to create directory in filesystem
        std::optional<std::pair<json::object, std::string>> folder_data_opt = db_conn->insert_folder(
            user_id,
            user_name,
            folder_name);

        // An error occured with database connection
        if (!folder_data_opt.has_value())
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "Internal server error occured");
        }

        // Create folder in file system
        std::filesystem::create_directory(folder_data_opt->second);

        // Initialize response
        response.body = json::serialize(folder_data_opt->first);
    }
    catch (const std::exception&)
    {
        return prepare_error_response(
            response, 
            http::status::unprocessable_entity, 
            "Invalid body format");
    }
}

void request_handlers::delete_folders(const request_params& request, response_params& response)
{
    std::vector<size_t> folder_ids;

    if (!uri_params::get_query_parameters(request.uri, "folderIds", folder_ids))
    {
        return prepare_error_response(
            response,
            http::status::unprocessable_entity, 
            "Invalid folder ids");
    }

    database_connection_wrapper db_conn = database_connections_pool::get();

    // No available connections
    if (!db_conn)
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "No available database connections");
    }
    
    // Try to delete given folders and get a pair of ids and paths of successfully deleted folders
    std::optional<std::pair<std::vector<size_t>, std::vector<std::string>>> deleted_folders_data_opt = 
        db_conn->delete_folders(folder_ids); 

    // An error occured with database connection
    if (!deleted_folders_data_opt.has_value())
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "Internal server error occured");
    }

    // After folders deletion from the database we delele them from the filesystem
    for (std::string& deleted_folder_path : deleted_folders_data_opt->second)
    {
        std::filesystem::remove_all(deleted_folder_path);
    }

    // Not all folders were deleted
    if (deleted_folders_data_opt->first.size() != folder_ids.size())
    {
        response.error_status = http::status::unprocessable_entity;
        response.body = json::serialize(
            json::object
            {
                {"error", "Some folders haven't been deleted"},
                {"deletedFolderIds", json::array(
                    deleted_folders_data_opt->first.begin(), 
                    deleted_folders_data_opt->first.end())}
            });
    }
}

void request_handlers::rename_folder(const request_params& request, response_params& response)
{
    try
    {
        size_t folder_id;

        if (!uri_params::get_path_parameter(request.uri, folder_id))
        {
            return prepare_error_response(
                response, 
                http::status::unprocessable_entity, 
                "Invalid folder id");
        }
        
        json::object body_json = json::parse(request.body).as_object();

        std::string new_folder_name = json::parse(request.body).as_object().at("newFolderName").as_string().c_str();

        // Trim whitespaces at the beggining and at the end of the folder name
        boost::algorithm::trim(new_folder_name);

        // Transform the string to UnicodeString to check the actual length of unicode symbols
        icu::UnicodeString new_folder_name_unicode{new_folder_name.c_str()}; 

        // Folder name can't be less than 3 and longer than 64 symbols
        if (new_folder_name_unicode.length() < 3 || new_folder_name_unicode.length() > 64) 
        {
            return prepare_error_response(
                response, 
                http::status::unprocessable_entity, 
                "Invalid new folder name format");
        }

        database_connection_wrapper db_conn = database_connections_pool::get();

        // No available connections
        if (!db_conn)
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "No available database connections");
        }

        std::optional<bool> does_folder_already_exist = db_conn->check_folder_existence_by_name(new_folder_name);

        // An error occured with database connection
        if (!does_folder_already_exist.has_value())
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "Internal server error occured");
        }

        // Folder with given name already exists
        if (does_folder_already_exist.value())
        {
            return prepare_error_response(
                response, 
                http::status::conflict, 
                "Folder with this name already exists");
        }

        // Rename folder by its id
        std::optional<bool> is_folder_id_valid_opt = db_conn->rename_folder(folder_id, new_folder_name);

        // An error occured with database connection
        if (!is_folder_id_valid_opt.has_value())
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "Internal server error occured");
        }

        // Folder with given id doesn't exist
        if (!is_folder_id_valid_opt.value())
        {
            return prepare_error_response(
                response, 
                http::status::not_found, 
                "Folder was not found");
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

void request_handlers::get_files_info(const request_params& request, response_params& response)
{
    size_t folder_id;

    if (!uri_params::get_query_parameters(request.uri, "folderId", folder_id))
    {
        return prepare_error_response(
            response,
            http::status::unprocessable_entity, 
            "Invalid folder id");
    }

    database_connection_wrapper db_conn = database_connections_pool::get();

    // No available connections
    if (!db_conn)
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "No available database connections");
    }
    
    // Get all files info in json format
    std::optional<json::object> files_info_json_opt = db_conn->get_files_info(folder_id); 

    // An error occured with database connection
    if (!files_info_json_opt.has_value())
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "Internal server error occured");
    }

    // Folder with given id doesn't exist
    if (files_info_json_opt->empty())
    {
        return prepare_error_response(
            response, 
            http::status::not_found, 
            "Folder was not found");
    }
    
    // Initialize body with string representation of json
    response.body = json::serialize(files_info_json_opt.value());
}

void request_handlers::process_downloaded_files(
    std::list<std::pair<size_t, std::string>>&& file_ids_and_paths,
    database_connection_wrapper&& db_conn)
{
    for (auto file_id_and_path : file_ids_and_paths)
    {
        db_conn->change_file_status(file_id_and_path.first, file_status::ready_for_parsing);
    }
}

void request_handlers::delete_files(const request_params& request, response_params& response)
{
    std::vector<size_t> file_ids;

    if (!uri_params::get_query_parameters(request.uri, "fileIds", file_ids))
    {
        return prepare_error_response(
            response,
            http::status::unprocessable_entity, 
            "Invalid file ids");
    }

    database_connection_wrapper db_conn = database_connections_pool::get();

    // No available connections
    if (!db_conn)
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "No available database connections");
    }
    
    // Try to delete given files and get a pair of ids and paths of successfully deleted files
    std::optional<std::pair<std::vector<size_t>, std::vector<std::string>>> deleted_files_data_opt = 
        db_conn->delete_files(file_ids); 

    // An error occured with database connection
    if (!deleted_files_data_opt.has_value())
    {
        return prepare_error_response(
            response, 
            http::status::internal_server_error, 
            "Internal server error occured");
    }

    // After files deletion from the database we delele them from the filesystem
    for (std::string& deleted_file_path : deleted_files_data_opt->second)
    {
        std::filesystem::remove(deleted_file_path);
    }

    // Not all files were deleted
    if (deleted_files_data_opt->first.size() != file_ids.size())
    {
        response.error_status = http::status::unprocessable_entity;
        response.body = json::serialize(
            json::object
            {
                {"error", "Some files haven't been deleted"},
                {"deletedFileIds", json::array(
                    deleted_files_data_opt->first.begin(), 
                    deleted_files_data_opt->first.end())}
            });
    }
}

void request_handlers::rename_file(const request_params& request, response_params& response)
{
    try
    {
        size_t file_id;

        if (!uri_params::get_path_parameter(request.uri, file_id))
        {
            return prepare_error_response(
                response, 
                http::status::unprocessable_entity, 
                "Invalid file id");
        }
        
        json::object body_json = json::parse(request.body).as_object();

        std::string new_file_name = json::parse(request.body).as_object().at("newFileName").as_string().c_str();

        // Trim whitespaces at the beggining and at the end of the file name
        boost::algorithm::trim(new_file_name);

        // Transform the string to UnicodeString to check the actual length of unicode symbols
        icu::UnicodeString new_file_name_unicode{new_file_name.c_str()}; 

        // File name can't be less than 3 and longer than 64 symbols
        if (new_file_name_unicode.length() < 3 || new_file_name_unicode.length() > 64) 
        {
            return prepare_error_response(
                response, 
                http::status::unprocessable_entity, 
                "Invalid new file name format");
        }

        database_connection_wrapper db_conn = database_connections_pool::get();

        // No available connections
        if (!db_conn)
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "No available database connections");
        }

        std::optional<size_t> folder_id_opt = db_conn->get_folder_id_by_file_id(file_id);

        // An error occured with database connection
        if (!folder_id_opt.has_value())
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "Internal server error occured");
        }

        // File with given id doesn't exist
        if (folder_id_opt.value() == 0)
        {
            return prepare_error_response(
                response, 
                http::status::not_found, 
                "File was not found");
        }

        std::optional<bool> does_file_already_exist = db_conn->check_file_existence_by_name(
            folder_id_opt.value(), 
            new_file_name);

        // An error occured with database connection
        if (!does_file_already_exist.has_value())
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "Internal server error occured");
        }

        // File with given name already exists in its folder
        if (does_file_already_exist.value())
        {
            return prepare_error_response(
                response,
                http::status::conflict, 
                "File with this name already exists in its folder");
        }

        // An error occured with database connection
        if (!db_conn->rename_file(file_id, new_file_name).has_value())
        {
            return prepare_error_response(
                response, 
                http::status::internal_server_error, 
                "Internal server error occured");
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