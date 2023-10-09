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
    _conn.reset();
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
    size_t user_id;
    
    try
    {
        user_id = transaction.query_value<size_t>(
            "SELECT id FROM users WHERE nickname=" + transaction.quote(username) + 
            " AND password=crypt(" + transaction.quote(password) + ",password)");
    }
    catch (const pqxx::broken_connection&)
    {
        transaction.abort();
        
        if (reconnect())
        {
            return login(username, password);
        }
        else
        {
            return {};
        }
    }
    // There are no users with given credentials
    catch (const pqxx::unexpected_rows&)
    {
        return 0;
    }
    catch (const std::exception&)
    {
        return {};
    } 

    return user_id;
}