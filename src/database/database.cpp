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

std::optional<std::string> database::login(const std::string_view& username, const std::string_view& password)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        return transaction.query_value<std::string>(
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
        return std::string{};
    }
    catch (const std::exception& ex)
    {
        LOG_ERROR << ex.what();
        return {};
    } 
}

std::optional<std::string> database::get_folder_path(const std::string& folder_id)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        // Check if the folder_id is an unsigned number and throw on fail
        pqxx::from_string<size_t>(folder_id);

        return transaction.query_value<std::string>(
            "SELECT path FROM folders WHERE id=" + folder_id);
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
    // Folder id is not an unsigned number
    catch (const pqxx::conversion_error&)
    {
        return std::string{};
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

std::optional<std::pair<std::string, std::string>> database::insert_file(
    const std::string& user_id, 
    const std::string& folder_id, 
    const std::string_view& filename)
{
    pqxx::work transaction{*_conn};
    
    try
    {
        // Check if the folder_id is an unsigned number and throw on fail
        pqxx::from_string<size_t>(folder_id);

        return transaction.query_value<std::string>(
            "WITH current_id AS (SELECT nextval('files_id_seq')) INSERT INTO files (id,name,extension,path,folder_id," 
            "uploaded_by_user_id) VALUES ((SELECT * FROM current_id)," + 
            transaction.quote(filename.remove_suffix(filename.find_last_of('.'))) +
            "," + config::FOLDERS_PATH + "(SELECT * FROM current_id)::text");
    }
    // Connection is lost
    catch (const pqxx::broken_connection& ex)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return insert_file(user_id, folder_id, filename);
        }
        else
        {
            LOG_ERROR << ex.what();
            return {};
        }
    }
    // Folder id is not an unsigned number
    catch (const pqxx::conversion_error&)
    {
        return std::string{};
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