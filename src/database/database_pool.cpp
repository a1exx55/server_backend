#include <database/database_pool.hpp>

void database_pool::init(
    size_t databases_number,
    std::string_view username, 
    std::string_view password, 
    std::string_view host, 
    size_t port,
    std::string_view database_name)
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