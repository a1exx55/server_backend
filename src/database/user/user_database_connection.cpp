#include <database/user/user_database_connection.hpp>

bool user_database_connection::close_all_sessions_except_current_impl(
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

std::optional<size_t> user_database_connection::login(std::string_view user_name, std::string_view password)
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

std::optional<std::monostate> user_database_connection::insert_session(
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

std::optional<bool> user_database_connection::close_current_session(
    std::string_view refresh_token, 
    std::string_view user_ip)
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

std::optional<bool> user_database_connection::update_refresh_token(
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

std::optional<json::object> user_database_connection::get_sessions_info(size_t user_id, std::string_view refresh_token)
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

std::optional<bool> user_database_connection::close_own_session(size_t session_id, size_t user_id)
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

std::optional<bool> user_database_connection::close_all_sessions_except_current(
    size_t user_id, 
    std::string_view refresh_token)
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

std::optional<bool> user_database_connection::validate_password(size_t user_id, std::string_view password)
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

std::optional<bool> user_database_connection::change_password(
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