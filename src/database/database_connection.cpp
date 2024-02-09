#include <database/database_connection.hpp>

database_connection::database_connection(
    std::string_view user_name, 
    std::string_view password, 
    std::string_view host, 
    size_t port,
    std::string_view database_name)
    : _conn{
        std::format(
            "user={} password={} host={} port={} dbname={}",
            user_name,
            password,
            host,
            port,
            database_name)}
{}

bool database_connection::reconnect()
{
    try
    {
        _conn.emplace(_conn->connection_string());
    }
    catch (const std::exception&)
    {
        return false;
    }
   
    return true;
}

bool database_connection::close_all_sessions_except_current_impl(
    pqxx::work& transaction, 
    size_t user_id,
    std::string_view refresh_token)
{
    try
    {
        size_t refresh_token_id = transaction.query_value<size_t>(
            std::format(
                "SELECT id FROM refresh_tokens "
                "WHERE token={}",
                transaction.quote(refresh_token)));
            
        transaction.exec0(
            std::format(
                "UPDATE sessions SET logout_date=LOCALTIMESTAMP,status='inactive' "
                "WHERE user_id={} AND refresh_token_id<>{}",
                user_id,
                refresh_token_id));

        transaction.exec0(
            std::format(
                "DELETE FROM refresh_tokens "
                "WHERE user_id={} AND id<>{}",
                user_id,
                refresh_token_id));
            
        transaction.commit();

        return true;
    }
    // Refresh token was not found
    catch (const pqxx::unexpected_rows&)
    {
        return false;
    }
}

bool database_connection::check_folder_existence_by_name_impl(
    pqxx::work& transaction, 
    std::string_view folder_name)
{
    // Check if the folder with given name already exists
    return transaction.query_value<bool>(
        std::format(    
            "SELECT EXISTS"
                "(SELECT 1 FROM folders "
                "WHERE name={})",
            transaction.quote(folder_name)));
}

bool database_connection::check_file_existence_by_name_impl(
    pqxx::work& transaction, 
    size_t folder_id, 
    std::string_view file_name)
{
    // Check if the file with given name already exists
    return transaction.query_value<bool>(
        std::format(
            "SELECT EXISTS"
                "(SELECT 1 FROM files "
                "WHERE folder_id={} AND name={})",
            folder_id,
            transaction.quote(file_name)));
}

std::optional<size_t> database_connection::login(std::string_view user_name, std::string_view password)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        return transaction.query_value<size_t>(
            std::format(
                "SELECT id FROM users "
                "WHERE nickname={} AND password=crypt({},password)",
                transaction.quote(user_name),
                transaction.quote(password)));
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return login(user_name, password);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    // There are no users with given credentials
    catch (const pqxx::unexpected_rows&)
    {
        return 0;
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<std::monostate> database_connection::insert_session(
    size_t user_id, 
    std::string_view refresh_token, 
    std::string_view user_agent,
    std::string_view user_ip,
    bool is_temporary_session)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        size_t refresh_token_id;

        // If session is temporary then we have to close the previous temporary session if there is because
        // there can be only one temporary session
        // If session in permanent then we have to close the oldest permanent session if there are more than 
        // 5 active permanent sessions(this is the restriction on active permanent sessions number)
        try
        {
            if (is_temporary_session)
            {
                refresh_token_id = transaction.query_value<size_t>(
                    std::format(
                        "SELECT refresh_token_id FROM sessions "
                        "WHERE user_id={} AND status='temp'",
                        user_id));
            }
            else
            {
                refresh_token_id = transaction.query_value<size_t>(
                    std::format(
                        "SELECT refresh_token_id FROM sessions "
                        "WHERE "
                            "(SELECT COUNT(*) FROM sessions "
                            "WHERE user_id={0} AND status='active')>=5 "
                        "AND id="
                            "(SELECT id FROM sessions "
                            "WHERE user_id={0} AND status='active' "
                            "ORDER BY last_seen_date "
                            "LIMIT 1)",
                        user_id));
            }

            transaction.exec0(
                std::format(
                    "UPDATE sessions SET logout_date=LOCALTIMESTAMP,ip={},status='inactive' "
                    "WHERE refresh_token_id={}",
                    transaction.quote(user_ip),
                    refresh_token_id));

            transaction.exec0(
                std::format(
                    "DELETE FROM refresh_tokens "
                    "WHERE id={}",
                    refresh_token_id));
        }
        // Either there is no temporary session yet or there are less than 5 active permanent sessions
        // so no need to close anything
        catch(const pqxx::unexpected_rows&)
        {}

        // Insert new refresh token
        refresh_token_id = transaction.query_value<size_t>(
            std::format(
                "INSERT INTO refresh_tokens (token,user_id) "
                "VALUES ({},{}) "
                "RETURNING id",
                transaction.quote(refresh_token),
                user_id));

        // Insert new session
        transaction.exec0(
            std::format(
                "INSERT INTO sessions (user_id,refresh_token_id,user_agent,ip,status) "
                "VALUES ({},{},{},{},{})",
                user_id,
                refresh_token_id,
                transaction.quote(user_agent),
                transaction.quote(user_ip),
                (is_temporary_session ? "'temp'" : "'active'")));
            
        transaction.commit();

        return std::monostate{};
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return insert_session(user_id, refresh_token, user_agent, user_ip, is_temporary_session);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<bool> database_connection::close_current_session(std::string_view refresh_token, std::string_view user_ip)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        // To close session we have to delete refresh token from corresponding table, update user ip and
        // change session status to 'inactive' and logout_date to current time
        size_t refresh_token_id = transaction.query_value<size_t>(
            std::format(
                "SELECT id FROM refresh_tokens "
                "WHERE token={}",
                transaction.quote(refresh_token)));
            
        transaction.exec0(
            std::format(
                "UPDATE sessions SET logout_date=LOCALTIMESTAMP,ip={},status='inactive' "
                "WHERE refresh_token_id={}",
                transaction.quote(user_ip),
                refresh_token_id));

        transaction.exec0(
            std::format(
                "DELETE FROM refresh_tokens "
                "WHERE id={}",
                refresh_token_id));
            
        transaction.commit();

        return true;
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return close_current_session(refresh_token, user_ip);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    // Refresh token was not found
    catch (const pqxx::unexpected_rows&)
    {
        return false;
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<bool> database_connection::update_refresh_token(
    std::string_view old_refresh_token, 
    std::string_view new_refresh_token, 
    std::string_view user_ip)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        // Except just updating refresh token we have to update session's info 
        // by changing last seen date to current time and updating user ip
        size_t refresh_token_id = transaction.query_value<size_t>(
            std::format(
                "UPDATE refresh_tokens SET token={} "
                "WHERE token={} "
                "RETURNING id",
                transaction.quote(new_refresh_token),
                transaction.quote(old_refresh_token)));
            
        transaction.exec0(
            std::format(
                "UPDATE sessions SET last_seen_date=LOCALTIMESTAMP,ip={} "
                "WHERE refresh_token_id={}",
                transaction.quote(user_ip),
                refresh_token_id));
            
        transaction.commit();

        return true;
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return update_refresh_token(old_refresh_token, new_refresh_token, user_ip);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    // Refresh token was not found
    catch (const pqxx::unexpected_rows&)
    {
        return false;
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<json::object> database_connection::get_sessions_info(size_t user_id, std::string_view refresh_token)
{
    pqxx::work transaction{*_conn};

    try
    {
        size_t refresh_token_id = transaction.query_value<size_t>(
            std::format(
                "SELECT id FROM refresh_tokens "
                "WHERE token={}",
                transaction.quote(refresh_token)));

        json::object sessions_json;

        // Use another scope because variables below will be defined again later
        {
            // Select current session's data and add it to json
            auto [id, login_date, last_seen_date, user_agent, ip] = 
                transaction.query1<size_t, std::string, std::string, std::string, std::string>(
                    std::format(
                        "SELECT id,login_date,last_seen_date,user_agent,ip FROM sessions "
                        "WHERE refresh_token_id={}",
                        refresh_token_id));
            
            sessions_json.emplace(
                "currentSession",
                json::object
                {
                    {"id", id},
                    {"loginDate", std::move(login_date)},
                    {"lastSeenDate", std::move(last_seen_date)},
                    {"userAgent", std::move(user_agent)},
                    {"ip", std::move(ip)}
                });
        }

        sessions_json.emplace("otherActiveSessions", json::array{});
        json::array& other_active_sessions_array = sessions_json.at("otherActiveSessions").as_array();
        
        // Select active sessions except current one and add them to json
        for (auto [id, login_date, last_seen_date, user_agent, ip] : 
            transaction.query<size_t, std::string, std::string, std::string, std::string>(
                std::format(
                    "SELECT id,login_date,last_seen_date,user_agent,ip FROM sessions "
                    "WHERE user_id={} AND status<>'inactive' AND refresh_token_id<>{} " 
                    "ORDER BY last_seen_date DESC",
                    user_id,
                    refresh_token_id)))
        {
            other_active_sessions_array.emplace_back(
                json::object
                {
                    {"id", id},
                    {"loginDate", std::move(login_date)},
                    {"lastSeenDate", std::move(last_seen_date)},
                    {"userAgent", std::move(user_agent)},
                    {"ip", std::move(ip)}
                });
        }   

        sessions_json.emplace("inactiveSessions", json::array{});
        json::array& inactive_sessions_array = sessions_json.at("inactiveSessions").as_array();

        // Select inactive sessions and add them to json
        for (auto [id, login_date, logout_date, user_agent, ip] : 
            transaction.query<size_t, std::string, std::string, std::string, std::string>(
                std::format(
                    "SELECT id,login_date,logout_date,user_agent,ip FROM sessions "
                    "WHERE user_id={} AND status='inactive' "
                    "ORDER BY logout_date DESC "
                    "LIMIT 15",
                    user_id)))
        {
            inactive_sessions_array.emplace_back(
                json::object
                {
                    {"id", id},
                    {"loginDate", std::move(login_date)},
                    {"logoutDate", std::move(logout_date)},
                    {"userAgent", std::move(user_agent)},
                    {"ip", std::move(ip)}
                });
        }    

        return sessions_json;    
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return get_sessions_info(user_id, refresh_token);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    // Refresh token was not found
    catch (const pqxx::unexpected_rows&)
    {
        return json::object{};
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<bool> database_connection::close_own_session(size_t session_id, size_t user_id)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        // User is able to close only own sessions so check if session with session_id 
        // belongs to user with user_id - query below will throw if session is not own
        size_t refresh_token_id = transaction.query_value<size_t>(
            std::format(
                "SELECT refresh_token_id FROM sessions "
                "WHERE id={} AND user_id={} AND status<>'inactive'",
                session_id,
                user_id));
            
        transaction.exec0(
            std::format(
                "UPDATE sessions SET logout_date=LOCALTIMESTAMP,status='inactive' "
                "WHERE refresh_token_id={}",
                refresh_token_id));

        transaction.exec0(
            std::format(
                "DELETE FROM refresh_tokens "
                "WHERE id={}",
                refresh_token_id));
            
        transaction.commit();

        return true;
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return close_own_session(session_id, user_id);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    // Either session with given id was not found or it doesn't belong to the user with given user id
    catch (const pqxx::unexpected_rows&)
    {
        return false;
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<bool> database_connection::close_all_sessions_except_current(size_t user_id, std::string_view refresh_token)
{
    pqxx::work transaction{*_conn};
  
    try
    {
        return close_all_sessions_except_current_impl(transaction, user_id, refresh_token);
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return close_all_sessions_except_current(user_id, refresh_token);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<bool> database_connection::validate_password(size_t user_id, std::string_view password)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        return transaction.query_value<bool>(
            std::format(
                "SELECT EXISTS"
                    "(SELECT 1 FROM users "
                    "WHERE id={} AND password=crypt({},password))",
                user_id,
                transaction.quote(password)));
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return validate_password(user_id, password);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<bool> database_connection::change_password(
    size_t user_id, 
    std::string_view new_password, 
    std::string_view refresh_token)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        transaction.exec0(
            std::format(
                "UPDATE users SET password=crypt({},gen_salt('bf',7)) "
                "WHERE id={}",
                transaction.quote(new_password),
                user_id));

        return close_all_sessions_except_current_impl(transaction, user_id, refresh_token);
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return change_password(user_id, new_password, refresh_token);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<json::array> database_connection::get_folders_info()
{
    pqxx::work transaction{*_conn};
    
    try
    {
        json::array folders_array;
        json::object folder_json;

        for (auto [id, name, last_upload_date, created_by, files_number] : 
            transaction.query<size_t, std::string, std::optional<std::string>, std::string, size_t>(
                "SELECT folders.id,folders.name,folders.last_upload_date,users.nickname,folders.files_number "
                "FROM folders "
                "JOIN users ON folders.created_by_user_id=users.id"))
        {
            folder_json = 
                json::object
                {
                    {"id", id},
                    {"name", name},
                    {"createdBy", created_by},
                    {"filesNumber", files_number}
                };

            // Last upload date may be null so process it separately
            if (last_upload_date.has_value())
            {
                folder_json.emplace("lastUploadDate", last_upload_date.value());
            }
            else
            {
                folder_json.emplace("lastUploadDate", nullptr);
            }

            folders_array.emplace_back(folder_json);
        }   
        
        transaction.commit();

        return folders_array;
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return get_folders_info();
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<bool> database_connection::check_folder_existence_by_name(std::string_view folder_name)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        return check_folder_existence_by_name_impl(
            transaction,
            folder_name);
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return check_folder_existence_by_name(folder_name);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    }
}

std::optional<std::pair<json::object, std::string>> database_connection::insert_folder(
    size_t user_id, 
    std::string_view user_name, 
    std::string_view folder_name)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        // Insert folder with given data
        auto [folder_id, folder_path] = transaction.query1<size_t, std::string>(
            std::format(
                "WITH current_id AS (SELECT nextval('folders_id_seq')) "
                    "INSERT INTO folders (id,name,path,created_by_user_id) "
                    "VALUES ((SELECT * FROM current_id),{},{} || (SELECT * FROM current_id)::text || '/',{}) "
                    "RETURNING id,path",
                transaction.quote(folder_name),
                transaction.quote(config::FOLDERS_PATH),
                user_id));

        json::object folder_data_json;

        // Construct json with data of newly inserted folder
        folder_data_json.emplace("id", folder_id);
        folder_data_json.emplace("name", folder_name);
        folder_data_json.emplace("lastUploadDate", nullptr);
        folder_data_json.emplace("createdBy", user_name);
        folder_data_json.emplace("filesNumber", 0);

        transaction.commit();

        return std::pair{folder_data_json, folder_path};
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return insert_folder(user_id, user_name, folder_name);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<std::pair<std::vector<size_t>, std::vector<std::string>>> database_connection::delete_folders(
    const std::vector<size_t>& folder_ids)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        std::vector<size_t> deleted_folder_ids;
        std::vector<std::string> deleted_folder_paths;

        std::string folder_ids_list;
        
        for (size_t folder_id : folder_ids)
        {
            folder_ids_list.append(std::to_string(folder_id) + ",");
        }

        folder_ids_list.erase(folder_ids_list.size() - 1);

        for (auto [deleted_folder_id, deleted_folder_path] : transaction.query<size_t, std::string>(
            std::format(
                "DELETE FROM folders "
                "WHERE id IN ({}) AND NOT "
                    "(SELECT EXISTS"
                        "(SELECT 1 FROM files "
                        "WHERE folder_id=folders.id AND status<>'ready_for_parsing')) "
                "RETURNING id,path",
                folder_ids_list)))
        {
            deleted_folder_ids.emplace_back(deleted_folder_id);
            deleted_folder_paths.emplace_back(std::move(deleted_folder_path));
        }

        transaction.commit();

        return std::pair{deleted_folder_ids, deleted_folder_paths};
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return delete_folders(folder_ids);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<bool> database_connection::rename_folder(size_t folder_id, std::string_view new_folder_name)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        _result = transaction.exec0(
            std::format(
                "UPDATE folders SET name={} "
                "WHERE id={}",
                transaction.quote(new_folder_name),
                folder_id));

        // Check if the update has occured i.e. folder with given id actually exists
        if (_result.affected_rows())
        {
            transaction.commit();
            return true;
        }
        else
        {
            return false;
        }
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return rename_folder(folder_id, new_folder_name);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<json::object> database_connection::get_files_info(size_t folder_id)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        // Get the folder name by its id or throw if folder with this id doesn't exist
        std::string folder_name = transaction.query_value<std::string>(
            std::format(
                "SELECT name FROM folders "
                "WHERE id={}",
                folder_id));

        // Create json for files data and add corresponding folder name
        json::object files_data_json
        {
            {"folderName", folder_name}
        };

        files_data_json.emplace("files", json::array{});
        json::array& files_data_array = files_data_json.at("files").as_array();

        json::object file_data_json;

        for (auto [id, name, size, upload_date, uploaded_by, status] : 
            transaction.query<size_t, std::string, std::optional<size_t>, std::optional<std::string>, std::string, std::string>(
                std::format(
                    "SELECT files.id,files.name ||'.'|| files.extension,files.size,files.upload_date,"
                        "users.nickname,files.status FROM files "
                    "JOIN users ON files.uploaded_by_user_id=users.id " 
                    "WHERE files.folder_id={}",
                    folder_id)))
        {
            file_data_json = 
                json::object
                {
                    {"id", id},
                    {"name", name},
                    {"uploadedBy", uploaded_by},
                    {"status", status}
                };

            // Size and upload date may be null so process them separately
            if (size.has_value())
            {
                file_data_json.emplace("size", size.value());
            }
            else
            {
                file_data_json.emplace("size", nullptr);
            }

            if (upload_date.has_value())
            {
                file_data_json.emplace("uploadDate", upload_date.value());
            }
            else
            {
                file_data_json.emplace("uploadDate", nullptr);
            }

            files_data_array.emplace_back(file_data_json);
        }   
        
        transaction.commit();

        return files_data_json;
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return get_files_info(folder_id);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    // Folder with given folder_id doesn't exist
    catch (const pqxx::unexpected_rows&)
    {
        return json::object{};
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<std::string> database_connection::get_file_path(size_t file_id)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        return transaction.query_value<std::string>(
            std::format(
                "SELECT path FROM files "
                "WHERE id={}",
                file_id));
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return get_file_path(file_id);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    // File with given id doesn't exist
    catch (const pqxx::unexpected_rows&)
    {
        return "";
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<bool> database_connection::check_folder_existence_by_id(size_t folder_id)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        return transaction.query_value<bool>(
            std::format(
                "SELECT EXISTS"
                    "(SELECT 1 FROM folders "
                    "WHERE id={})",
                folder_id));
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return check_folder_existence_by_id(folder_id);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<bool> database_connection::check_file_existence_by_name(size_t folder_id, std::string_view file_name)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        return check_file_existence_by_name_impl(
            transaction,
            folder_id,
            file_name);
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return check_file_existence_by_name(folder_id, file_name);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    }
}

std::optional<std::tuple<size_t, std::filesystem::path, std::string>> database_connection::insert_uploading_file(
    size_t user_id, 
    size_t folder_id,
    std::string_view file_name,
    std::string_view file_extension)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        auto [file_id, file_path] = transaction.query1<size_t, std::string>(
            std::format(
                "WITH current_id AS (SELECT nextval('files_id_seq')) "
                    "INSERT INTO files (id,name,extension,path,folder_id,uploaded_by_user_id) "
                    "VALUES ((SELECT * FROM current_id),{0},{1},{2}||{3}||'/'||(SELECT * FROM current_id)::text||'.'||{1},{3},{4}) "
                    "RETURNING id,path",
                transaction.quote(file_name),
                transaction.quote(file_extension),
                transaction.quote(config::FOLDERS_PATH),
                folder_id,
                user_id));

        transaction.commit();

        return std::tuple<size_t, std::filesystem::path, std::string>{file_id, file_path, file_extension};
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return insert_uploading_file(user_id, folder_id, file_name, file_extension);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    }
}

std::optional<std::monostate> database_connection::delete_file(size_t file_id)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        transaction.exec0(
            std::format(
                "DELETE FROM files "
                "WHERE id={}",
                file_id));
        
        transaction.commit();

        return std::monostate{};
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return delete_file(file_id);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    }
}

std::optional<std::monostate> database_connection::update_uploaded_file(size_t file_id, size_t file_size)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        transaction.exec0(
            std::format(
                "UPDATE files SET size={},upload_date=LOCALTIMESTAMP,status='uploaded' "
                "WHERE id={}",
                file_size,
                file_id));
        
        transaction.commit();
    
        return std::monostate{};
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return update_uploaded_file(file_id, file_size);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    }
}

std::optional<std::tuple<size_t, std::filesystem::path, std::string>> database_connection::insert_unzipped_file(
    size_t user_id,
    size_t folder_id,
    std::string_view file_name,
    std::string_view file_extension,
    size_t file_size)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        auto [file_id, file_path] = transaction.query1<size_t, std::string>(
            std::format(
                "WITH current_id AS (SELECT nextval('files_id_seq')) "
                    "INSERT INTO files (id,name,extension,path,folder_id,size,upload_date,uploaded_by_user_id,status) "
                    "VALUES ((SELECT * FROM current_id),{0},{1},{2}||{3}||'/'||(SELECT * FROM current_id)::text||'.'||"
                        "{1},{3},{4},LOCALTIMESTAMP,{5},'uploaded') "
                    "RETURNING id,path",
                transaction.quote(file_name),
                transaction.quote(file_extension),
                transaction.quote(config::FOLDERS_PATH),
                folder_id,
                file_size,
                user_id));

        transaction.commit();

        return std::tuple<size_t, std::filesystem::path, std::string>{file_id, file_path, file_extension};
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return insert_unzipped_file(user_id, folder_id, file_name, file_extension, file_size);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    }
}

std::optional<std::monostate> database_connection::change_file_status(size_t file_id, file_status new_status)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        transaction.exec0(
            std::format(
                "UPDATE files SET status={} " 
                "WHERE id={}",
                transaction.quote(magic_enum::enum_name(new_status)),
                file_id));
        
        transaction.commit();
    
        return std::monostate{};
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return change_file_status(file_id, new_status);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    }
}

std::optional<std::monostate> database_connection::update_converted_file(
    size_t file_id, 
    std::string_view new_file_extension, 
    std::string_view new_file_path, 
    size_t new_file_size)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        // Update extension and path replacing the old extension to the new one
        transaction.exec0(
            std::format(
                "UPDATE files SET extension={},path={},size={} " 
                "WHERE id={}",
                transaction.quote(new_file_extension),
                transaction.quote(new_file_path),
                new_file_size,
                file_id));
        
        transaction.commit();
    
        return std::monostate{};
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return update_converted_file(file_id, new_file_extension, new_file_path, new_file_size);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    }
}

std::optional<std::pair<std::vector<size_t>, std::vector<std::string>>> database_connection::delete_files(
    const std::vector<size_t>& file_ids)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        std::vector<size_t> deleted_file_ids;
        std::vector<std::string> deleted_file_paths;

        std::string file_ids_list;
        
        for (size_t file_id : file_ids)
        {
            file_ids_list.append(std::to_string(file_id) + ",");
        }

        file_ids_list.erase(file_ids_list.size() - 1);

        for (auto [deleted_file_id, deleted_file_path] : transaction.query<size_t, std::string>(
            std::format(
                "DELETE FROM files "
                "WHERE id IN ({}) AND status='ready_for_parsing' "
                "RETURNING id,path",
                file_ids_list)))
        {
            deleted_file_ids.emplace_back(deleted_file_id);
            deleted_file_paths.emplace_back(std::move(deleted_file_path));
        }

        transaction.commit();

        return std::pair{deleted_file_ids, deleted_file_paths};
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return delete_files(file_ids);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<size_t> database_connection::get_folder_id_by_file_id(size_t file_id)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        return transaction.query_value<size_t>(
            std::format(
                "SELECT folder_id FROM files "
                "WHERE id={}",
                file_id));
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return get_folder_id_by_file_id(file_id);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    // File with given id doesn't exist
    catch (const pqxx::unexpected_rows&)
    {
        return 0;
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<bool> database_connection::rename_file(size_t file_id, std::string_view new_file_name)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        _result = transaction.exec0(
            std::format(
                "UPDATE files SET name={} "
                "WHERE id={}",
                transaction.quote(new_file_name),
                file_id));

        // Check if the update has occured i.e. file with given id actually exists
        if (_result.affected_rows())
        {
            transaction.commit();
            return true;
        }
        else
        {
            return false;
        }
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return rename_file(file_id, new_file_name);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}