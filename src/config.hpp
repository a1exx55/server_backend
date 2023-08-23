#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <stdint.h>
#include <chrono>

namespace config
{
    inline constexpr std::string SERVER_IP_ADDRESS{"127.0.0.1"};
    inline constexpr uint_least16_t SERVER_PORT{12345};
    inline constexpr int THREADS_NUMBER{20};
    inline const std::string LOG_FILE_PATH{"/home/dmitry/vscodeprojects/dataleak_parser/server_backend.log"};
    inline constexpr bool CONSOLE_LOG_ENABLED{true};
    inline constexpr std::string JWT_SECRET_KEY{"secret_key"};
    inline constexpr std::chrono::minutes ACCESS_TOKEN_EXPIRY_TIME{15};
    inline constexpr std::chrono::days REFRESH_TOKEN_EXPIRY_TIME{30};
}

#endif