#ifndef DATABASE_CONNECTIONS_POOL_HPP
#define DATABASE_CONNECTIONS_POOL_HPP

//local
#include <database/database_connection.hpp>

//internal
#include <stack>
#include <memory>
#include <mutex>

// Special wrapper for the database_connection class to automatically return connection to the pool 
// after wrapper destruction. Allows you to work with it as with pointer to the database_connection 
// to invoke all the database methods and check if the connection is valid
// Support only move semantics(no copy) as it stores std::unique_ptr to the database connection 
// that allows only moving itself too
class database_connection_wrapper
{
    public:
        database_connection_wrapper()
            : _db_conn_ptr{}
        {}

        database_connection_wrapper(std::unique_ptr<database_connection>&& database_connection_ptr)
            : _db_conn_ptr(std::move(database_connection_ptr))
        {}

        database_connection_wrapper(const database_connection_wrapper& other_wrapper) = delete;

        database_connection_wrapper(database_connection_wrapper&& other_wrapper) = default;

        // Release this wrapper with stored database connection before the actual destruction
        ~database_connection_wrapper();
        
        // Get the reference to the stored pointer to database connection
        std::unique_ptr<database_connection>& operator*()
        {
            return _db_conn_ptr;
        }

        // Get access to the database_connection methods 
        database_connection* operator->()
        {
            return _db_conn_ptr.get();
        }

        // Check if the stored database connection is valid or even exist
        operator bool() const
        {
            return _db_conn_ptr.operator bool();
        }

    private:
        std::unique_ptr<database_connection> _db_conn_ptr;
};


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
            std::string_view database_name);

        // Return database_connection_wrapper object from the pool that is just the wrap to the actual connection
        // so it allows to handle it as the pointer to the database_connection
        // If there are no free database connections in the pool then it return empty database_connection_wrapper
        // that can be checked with bool operator
        static database_connection_wrapper get();

        // Release the database_connection_wrapper object to the pool if there is actual database_connection inside
        // After this operation the database_connection_wrapper object is not valid
        static void release(database_connection_wrapper&& database_connection);

    private:
        inline static std::stack<std::unique_ptr<database_connection>> _db_conn_ptrs_pool{};
        inline static std::mutex _mutex{};
};

#endif