#ifndef CONFIG_HPP
#define CONFIG_HPP

//internal
#include <string>
#include <stdint.h>
#include <chrono>

namespace config
{
    inline constexpr std::string SERVER_IP_ADDRESS{"127.0.0.1"};
    inline constexpr uint_least16_t SERVER_PORT{12345};
    inline constexpr int THREADS_NUMBER{20};
    inline constexpr size_t DATABASES_NUMBER{20};
    inline constexpr size_t DATABASE_PORT{5432};
    inline constexpr std::string DATABASE_NAME{"drain"};
    inline constexpr std::string DATABASE_USERNAME{"root"};
    inline constexpr std::string DATABASE_PASSWORD{"oral"};
    inline const std::string FOLDERS_PATH{"/home/dmitry/drains/"};
    inline const std::string LOG_FILE_PATH{"/home/dmitry/vscodeprojects/dataleak_parser/server_backend.log"};
    inline constexpr bool CONSOLE_LOG_ENABLED{true};
    inline constexpr std::string JWT_SECRET_KEY{"secret_key"};
    inline constexpr std::chrono::minutes ACCESS_TOKEN_EXPIRY_TIME{15};
    inline constexpr std::chrono::days REFRESH_TOKEN_EXPIRY_TIME{30};
}

#endif