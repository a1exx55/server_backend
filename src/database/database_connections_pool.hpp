#ifndef DATABASE_CONNECTIONS_POOL_HPP
#define DATABASE_CONNECTIONS_POOL_HPP

//local
#include <database/database_connection.hpp>

//internal
#include <stack>
#include <memory>
#include <mutex>
#include <type_traits>

// Special wrapper for the database_connection child class to automatically return connection to the pool 
// after wrapper destruction. Allows to work with it as with pointer to the database_connection child
// to invoke all the corresponding database methods and check if the connection is valid
// Support only move semantics(no copy) as it stores database connection that allows only moving too
// std::is_move_constructible<database_connection>
template <typename T>
requires std::derived_from<T, database_connection> 
class database_connection_wrapper
{
    public:
        database_connection_wrapper(){}

        database_connection_wrapper(database_connection&& database_connection)
            : _db_conn(std::move(database_connection)){}

        database_connection_wrapper(const database_connection_wrapper& other_wrapper) = delete;

        database_connection_wrapper(database_connection_wrapper&& other_wrapper) = default;

        // Release this wrapper with stored database connection before the actual destruction
        ~database_connection_wrapper();
        
        // Get access to the database_connection child methods 
        T* operator->()
        {
            return &_db_conn;
        }

        // Check if the stored database connection is valid
        operator bool()
        {
            return _db_conn;
        }

        // Release the stored database connection
        T release()
        {
            return std::move(_db_conn);
        }

    private:
        T _db_conn;
};


// Pool for storing connections to database which allows to initialize, get and release connections
class database_connections_pool
{
    public:
        // Initialize pool with given database connections number 
        // Each database_connection is initialized with given parameters
        // Since database_connection class constructor can throw exception, database_connections_pool can do it either
        static void init(
            size_t database_connections_number,
            std::string_view username, 
            std::string_view password, 
            std::string_view host, 
            size_t port,
            std::string_view database_name)
        {
            for (size_t i = 0; i < database_connections_number; ++i)
            {
                _db_conns_pool.emplace(username, password, host, port, database_name);
            }
        }

        // Return database_connection_wrapper object from the pool that is just the wrap to the certain connection
        // class so it allows to handle it as the pointer to the database_connection child
        // If there are no free database connections in the pool then it return empty database_connection_wrapper
        // that can be checked with bool operator
        template <typename T>
        static database_connection_wrapper<T> get()
        {
            std::lock_guard<std::mutex> lock(_mutex);

            // If there are no free database connections then return empty wrapper to process it outside
            if (_db_conns_pool.empty())
            {
                return database_connection_wrapper<T>();
            }
            
            // If found free database connection then take it from the pool, create wrapper out of it and return
            database_connection_wrapper<T> free_db_conn_wrapper = std::move(_db_conns_pool.top());
            _db_conns_pool.pop();

            return free_db_conn_wrapper;
        }

        // Release the database_connection_wrapper object to the pool if there is actual database connection inside
        // After this operation the database_connection_wrapper object is not valid
        template <typename T>
        static void release(database_connection_wrapper<T>&& wrapped_database_connection)
        {
            // If wrapped database connection is not empty then return it to the pool
            if (wrapped_database_connection)
            {
                std::lock_guard<std::mutex> lock(_mutex);
                
                _db_conns_pool.emplace(wrapped_database_connection.release());
            }
        }

    private:
        inline static std::stack<database_connection> _db_conns_pool{};
        inline static std::mutex _mutex{};
};

template <typename T>
requires std::derived_from<T, database_connection>
database_connection_wrapper<T>::~database_connection_wrapper()
{
    // Release this wrapper if there is actual database connection
    if (_db_conn)
    {
        database_connections_pool::release(std::move(*this));
    }
}

#endif