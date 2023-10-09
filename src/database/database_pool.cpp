#include <database/database_pool.hpp>

void database_pool::init(
    const size_t databases_number,
    const std::string& username, 
    const std::string& password, 
    const std::string& host, 
    const size_t port,
    const std::string& database_name)
{
    for (size_t i = 0; i < databases_number; ++i)
    {
        _db_pool.emplace(std::make_unique<database>(username, password, host, port, database_name));
    }
}

std::unique_ptr<database> database_pool::get()
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_db_pool.empty())
    {
        return std::unique_ptr<database>();
    }
    
    std::unique_ptr<database> db{std::move(_db_pool.top())};
    _db_pool.pop();

    return db;
}

void database_pool::release(std::unique_ptr<database> database)
{
    _db_pool.push(std::move(database));
}