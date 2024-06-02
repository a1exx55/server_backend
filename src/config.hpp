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
    inline const std::string CONFIG_PATH{"../config.json"};

    inline std::string SERVER_IP_ADDRESS;
    inline uint_least16_t SERVER_PORT;
    inline std::string DOMAIN_NAME;
    inline int THREADS_NUMBER;
    inline size_t DATABASE_CONNECTIONS_NUMBER;
    inline size_t DATABASE_PORT;
    inline std::string DATABASE_NAME;
    inline std::string DATABASE_USERNAME;
    inline std::string DATABASE_PASSWORD;
    inline std::string FOLDERS_PATH;
    inline std::string LOG_FILE_PATH;
    inline std::string SSL_CERT_PATH;
    inline std::string SSL_KEY_PATH;
    inline bool CONSOLE_LOG_ENABLED;
    inline std::string JWT_SECRET_KEY;
    inline std::chrono::minutes ACCESS_TOKEN_EXPIRY_TIME_MINUTES;
    inline std::chrono::days REFRESH_TOKEN_EXPIRY_TIME_DAYS;
    inline std::unordered_set<std::string> ALLOWED_UPLOADING_FILE_EXTENSIONS;
    inline std::unordered_set<std::string> ALLOWED_ARCHIVE_EXTENSIONS;
    inline std::unordered_set<std::string> ALLOWED_PARSING_FILE_EXTENSIONS;
    inline std::string PATH_TO_7ZIP_LIB;
    // Number of rows that is being examined to determine the type of file
    inline size_t ROWS_NUMBER_TO_EXAMINE;
    // Assumed maximum number of bytes that any row can contain in file of any type
    // This option is necessary to process file reading it into buffer by chunks 
    // to know whether the left part of buffer can represent the row or not
    inline size_t MAX_BYTES_NUMBER_IN_ROW;
    // The maximum rows number that each normalized file can contain 
    inline size_t MAX_ROWS_NUMBER_IN_NORMALIZED_FILE;

    inline void init()
    {
        std::ifstream config_file{CONFIG_PATH};

        if (!config_file.is_open())
        {
            throw std::invalid_argument{
                "Config file was not found at the specified path: " + 
                std::filesystem::canonical(config::CONFIG_PATH).string()};
        }
        
        std::string config_data;
        size_t config_file_size = std::filesystem::file_size(CONFIG_PATH);
        config_data.resize(config_file_size);

        config_file.read(config_data.data(), config_file_size);
        json::object config_json = json::parse(config_data).as_object();
        
        // Initialize config variables with values from json
        SERVER_IP_ADDRESS = config_json.at("SERVER_IP_ADDRESS").as_string();
        SERVER_PORT = config_json.at("SERVER_PORT").to_number<uint_least16_t>();
        DOMAIN_NAME = config_json.at("DOMAIN_NAME").as_string();
        THREADS_NUMBER = config_json.at("THREADS_NUMBER").to_number<int>();
        DATABASE_CONNECTIONS_NUMBER = config_json.at("DATABASE_CONNECTIONS_NUMBER").to_number<size_t>();
        DATABASE_PORT = config_json.at("DATABASE_PORT").to_number<size_t>();
        DATABASE_NAME = config_json.at("DATABASE_NAME").as_string();
        DATABASE_USERNAME = config_json.at("DATABASE_USERNAME").as_string();
        DATABASE_PASSWORD = config_json.at("DATABASE_PASSWORD").as_string();
        FOLDERS_PATH = config_json.at("FOLDERS_PATH").as_string();
        LOG_FILE_PATH = config_json.at("LOG_FILE_PATH").as_string();
        SSL_CERT_PATH = config_json.at("SSL_CERT_PATH").as_string();
        SSL_KEY_PATH = config_json.at("SSL_KEY_PATH").as_string();
        CONSOLE_LOG_ENABLED = config_json.at("CONSOLE_LOG_ENABLED").as_bool();
        JWT_SECRET_KEY = config_json.at("JWT_SECRET_KEY").as_string();
        ACCESS_TOKEN_EXPIRY_TIME_MINUTES = std::chrono::minutes{
            config_json.at("ACCESS_TOKEN_EXPIRY_TIME_MINUTES").to_number<size_t>()};
        REFRESH_TOKEN_EXPIRY_TIME_DAYS = std::chrono::days{
            config_json.at("REFRESH_TOKEN_EXPIRY_TIME_DAYS").to_number<size_t>()};
        for (auto file_extension : config_json.at("ALLOWED_UPLOADING_FILE_EXTENSIONS").as_array())
        {
            ALLOWED_UPLOADING_FILE_EXTENSIONS.emplace(file_extension.as_string());
        }
        for (auto archive_extension : config_json.at("ALLOWED_ARCHIVE_EXTENSIONS").as_array())
        {
            ALLOWED_ARCHIVE_EXTENSIONS.emplace(archive_extension.as_string());
        }
        for (auto file_extension : config_json.at("ALLOWED_PARSING_FILE_EXTENSIONS").as_array())
        {
            ALLOWED_PARSING_FILE_EXTENSIONS.emplace(file_extension.as_string());
        }
        PATH_TO_7ZIP_LIB = config_json.at("PATH_TO_7ZIP_LIB").as_string();
        ROWS_NUMBER_TO_EXAMINE = config_json.at("ROWS_NUMBER_TO_EXAMINE").to_number<size_t>();
        MAX_BYTES_NUMBER_IN_ROW = config_json.at("MAX_BYTES_NUMBER_IN_ROW").to_number<size_t>();
        MAX_ROWS_NUMBER_IN_NORMALIZED_FILE = config_json.at("MAX_ROWS_NUMBER_IN_NORMALIZED_FILE").to_number<size_t>();
    }
}

#endif