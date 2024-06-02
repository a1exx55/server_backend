#ifndef USER_DATABASE_CONNECTION_HPP
#define USER_DATABASE_CONNECTION_HPP

//local
#include <database/database_connection.hpp>
#include <logging/logger.hpp>

//internal
#include <optional>
#include <format>

//external
#include <boost/json.hpp>
#include <magic_enum.hpp>

namespace json = boost::json;   

class user_database_connection : public database_connection
{
    public:
        // Check if the user with given user_name and password exists in database
        // Return the user id on finding, otherwise return 0(user ids start with 1)
        // Return empty std::optional on fail
        std::optional<size_t> login(std::string_view user_name, std::string_view password);

        // Insert refresh token to the 'refresh_tokens' table and insert session to the 'sessions' table with given data
        // Temporary session can be only one so before inserting we close the previous one
        // Return empty std::optional on fail
        std::optional<std::monostate> insert_session(
            size_t user_id, 
            std::string_view refresh_token, 
            std::string_view user_agent,
            std::string_view user_ip,
            bool is_temporary_session);

        std::optional<bool> close_current_session(std::string_view refresh_token, std::string_view user_ip);

        std::optional<bool> update_refresh_token(
            std::string_view old_refresh_token, 
            std::string_view new_refresh_token, 
            std::string_view user_ip);

        std::optional<json::object> get_sessions_info(size_t user_id, std::string_view refresh_token);

        std::optional<bool> close_own_session(size_t session_id, size_t user_id);

        std::optional<bool> close_all_sessions_except_current(size_t user_id, std::string_view refresh_token);

        std::optional<bool> validate_password(size_t user_id, std::string_view password);

        std::optional<bool> change_password(
            size_t user_id, 
            std::string_view new_password, 
            std::string_view refresh_token);
    private:
        bool close_all_sessions_except_current_impl(
            pqxx::work& transaction, 
            size_t user_id, 
            std::string_view refresh_token); 
};

#endif