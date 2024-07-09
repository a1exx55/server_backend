#ifndef CONFIG_HPP
#define CONFIG_HPP

//internal
#include <string>
#include <stdint.h>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <unordered_set>

//external
#include <boost/json.hpp>

namespace json = boost::json;

namespace config
{
    // Path to the json config
    inline const std::string config_path{"../config.json"};

    inline std::string server_ip_address;
    inline uint_least16_t server_port;
    inline std::string domain_name;
    inline int threads_number;
    inline size_t database_connections_number;
    inline size_t database_port;
    inline std::string database_name;
    inline std::string database_username;
    inline std::string database_password;
    inline std::string folders_path;
    inline std::string log_file_path;
    inline std::string ssl_cert_path;
    inline std::string ssl_key_path;
    inline bool console_log_enabled;
    inline std::string jwt_secret_key;
    inline std::chrono::minutes access_token_expiry_time_minutes;
    inline std::chrono::days refresh_token_expiry_time_days;
    inline std::chrono::seconds operations_timeout;
    inline std::unordered_set<std::string> allowed_uploading_file_extensions;
    inline std::unordered_set<std::string> allowed_archive_extensions;
    inline std::unordered_set<std::string> allowed_parsing_file_extensions;
    inline std::string path_to_7zip_lib;
    // Number of rows that is being examined to determine the type of file
    inline size_t rows_number_to_examine;
    // Assumed maximum number of bytes that any row can contain in file of any type
    // This option is necessary to process file reading it into buffer by chunks 
    // to know whether the left part of buffer can represent the row or not
    inline size_t max_bytes_number_in_row;
    // The maximum rows number that each normalized file can contain 
    inline size_t max_rows_number_in_normalized_file;

    inline void init()
    {
        std::ifstream config_file{config_path};

        if (!config_file.is_open())
        {
            throw std::invalid_argument{
                "Config file was not found at the specified path: " + 
                std::filesystem::canonical(config::config_path).string()};
        }
        
        std::string config_data;
        size_t config_file_size = std::filesystem::file_size(config_path);
        config_data.resize(config_file_size);

        config_file.read(config_data.data(), config_file_size);
        json::object config_json = json::parse(config_data).as_object();
        
        // Initialize config variables with values from json
        server_ip_address = config_json.at("server_ip_address").as_string();
        server_port = config_json.at("server_port").to_number<uint_least16_t>();
        domain_name = config_json.at("domain_name").as_string();
        threads_number = config_json.at("threads_number").to_number<int>();
        database_connections_number = config_json.at("database_connections_number").to_number<size_t>();
        database_port = config_json.at("database_port").to_number<size_t>();
        database_name = config_json.at("database_name").as_string();
        database_username = config_json.at("database_username").as_string();
        database_password = config_json.at("database_password").as_string();
        folders_path = config_json.at("folders_path").as_string();
        log_file_path = config_json.at("log_file_path").as_string();
        ssl_cert_path = config_json.at("ssl_cert_path").as_string();
        ssl_key_path = config_json.at("ssl_key_path").as_string();
        console_log_enabled = config_json.at("console_log_enabled").as_bool();
        jwt_secret_key = config_json.at("jwt_secret_key").as_string();
        access_token_expiry_time_minutes = std::chrono::minutes{
            config_json.at("access_token_expiry_time_minutes").to_number<size_t>()};
        refresh_token_expiry_time_days = std::chrono::days{
            config_json.at("refresh_token_expiry_time_days").to_number<size_t>()};
        operations_timeout = std::chrono::seconds{
            config_json.at("operations_timeout").to_number<size_t>()};
        for (const auto& file_extension : config_json.at("allowed_uploading_file_extensions").as_array())
        {
            allowed_uploading_file_extensions.emplace(file_extension.as_string());
        }
        for (const auto& archive_extension : config_json.at("allowed_archive_extensions").as_array())
        {
            allowed_archive_extensions.emplace(archive_extension.as_string());
        }
        for (const auto& file_extension : config_json.at("allowed_parsing_file_extensions").as_array())
        {
            allowed_parsing_file_extensions.emplace(file_extension.as_string());
        }
        path_to_7zip_lib = config_json.at("path_to_7zip_lib").as_string();
        rows_number_to_examine = config_json.at("rows_number_to_examine").to_number<size_t>();
        max_bytes_number_in_row = config_json.at("max_bytes_number_in_row").to_number<size_t>();
        max_rows_number_in_normalized_file = config_json.at("max_rows_number_in_normalized_file").to_number<size_t>();
    }
}

#endif