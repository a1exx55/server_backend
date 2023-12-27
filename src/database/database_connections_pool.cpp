#include <database/database_connections_pool.hpp>

// Implement destructor outside the class to avoid circular dependency with database_connections_pool
database_connection_wrapper::~database_connection_wrapper()
{
    // Release this wrapper if there is actual database connection
    if (_db_conn_ptr)
    {
        database_connections_pool::release(std::move(*this));
    }
}

void database_connections_pool::init(
    size_t database_connections_number,
    std::string_view username, 
    std::string_view password, 
    std::string_view host, 
    size_t port,
    std::string_view database_name)
{
    for (size_t i = 0; i < database_connections_number; ++i)
    {
        _db_conn_ptrs_pool.emplace(
            std::make_unique<database_connection>(username, password, host, port, database_name));
    }
}

database_connection_wrapper database_connections_pool::get()
{
    std::lock_guard<std::mutex> lock(_mutex);

    // If there are no free database connections then return empty wrapper to process it outside
    if (_db_conn_ptrs_pool.empty())
    {
        return database_connection_wrapper();
    }
    
    // If found free database connection then take it from the pool, create wrapper out of it and return
    database_connection_wrapper free_db_conn = std::move(_db_conn_ptrs_pool.top());
    _db_conn_ptrs_pool.pop();

    return free_db_conn;
}

void database_connections_pool::release(database_connection_wrapper&& wrapped_database_connection)
{
    // If wrapped database connection is not empty then return it to the pool
    if (wrapped_database_connection)
    {
        _db_conn_ptrs_pool.push(std::move(*wrapped_database_connection));
    }
}