#ifndef DATABASE_POOL_HPP
#define DATABASE_POOL_HPP

//local
#include <database/database.hpp>

//internal
#include <stack>
#include <memory>
#include <mutex>

class database_pool
{
    public:
        // Initialize pool with given databases number 
        // Each database is initialized with given parameters
        // Since database class constructor can throw exception, database_pool can do it either
        static void init(
            const size_t databases_number,
            const std::string& username, 
            const std::string& password, 
            const std::string& host, 
            const size_t port,
            const std::string& database_name);

        // Return database object from the pool wrapped in unique_ptr
        // If there are no free databases in the pool then it return empty constructed unique_ptr(nullptr)
        static std::unique_ptr<database> get();

        // Release the database object to the pool 
        // After this operation the database object is not valid
        static void release(std::unique_ptr<database> database);

    private:
        inline static std::stack<std::unique_ptr<database>> _db_pool{};
        inline static std::mutex _mutex{};
};

#endif