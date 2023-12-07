#include <database/database.hpp>

database::database(
            const std::string& username, 
            const std::string& password, 
            const std::string& host, 
            const size_t port,
            const std::string& database_name)
            : _conn{"user=" + username + " password=" + password + 
                " host=" + host + " port=" + std::to_string(port) + " dbname=" + database_name}
              //_nontrans{*_conn}
{}

bool database::reconnect()
{
    std::string conn_string{_conn->connection_string()};
    try
    {
        _conn.emplace(conn_string);
    }
    catch (const std::exception&)
    {
        return false;
    }
   
    return true;
}

std::optional<size_t> database::login(const std::string_view& username, const std::string_view& password)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        return transaction.query_value<size_t>(
            "SELECT id FROM users WHERE nickname=" + transaction.quote(username) + 
            " AND password=crypt(" + transaction.quote(password) + ",password)");
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return login(username, password);
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

std::optional<std::string> database::get_folder_path(size_t folder_id)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        return transaction.query_value<std::string>(
            "SELECT path FROM folders WHERE id=" + pqxx::to_string(folder_id));
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return get_folder_path(folder_id);
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
        return std::string{};
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<bool> database::check_file_existence_by_name(size_t folder_id, const std::string_view& file_name)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        return transaction.query_value<bool>("SELECT EXISTS(SELECT 1 FROM files WHERE folder_id=" + 
            pqxx::to_string(folder_id) + " AND name=" + transaction.quote(file_name) + ")");
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

std::optional<std::pair<size_t, std::string>> database::insert_file(
    size_t user_id, 
    size_t folder_id,
    const std::string_view& folder_path,
    const std::string_view& file_name,
    const std::string_view& file_extension)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        auto [file_id, file_path] = transaction.query1<size_t, std::string>(
            "WITH current_id AS (SELECT nextval('files_id_seq')) INSERT INTO files (id,name,extension," 
            "path,folder_id,uploaded_by_user_id) VALUES ((SELECT * FROM current_id)," + 
            transaction.quote(file_name) + "," + transaction.quote(file_extension) + "," + 
            transaction.quote(folder_path) + "||(SELECT * FROM current_id)::text||'.'||" + 
            transaction.quote(file_extension) + "," + pqxx::to_string(folder_id) + "," + 
            pqxx::to_string(user_id) + ") RETURNING id, path");

        transaction.commit();

        return {{file_id, file_path}};
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return insert_file(user_id, folder_id, folder_path, file_name, file_extension);
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

std::optional<bool> database::delete_file(size_t file_id)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        _result = transaction.exec0("DELETE FROM files WHERE id=" + pqxx::to_string(file_id));
        
        transaction.commit();

        if (_result.affected_rows())
        {
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

std::optional<bool> database::update_uploaded_file(size_t file_id, size_t file_size)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        transaction.exec0("UPDATE files SET size=" + pqxx::to_string(file_size) +
            ",upload_date=LOCALTIMESTAMP,status='uploaded' WHERE id=" + pqxx::to_string(file_id));
        
        transaction.commit();
    
        if (_result.affected_rows())
        {
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