#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <stdint.h>

namespace config
{
    const std::string SERVER_IP_ADDRESS("127.0.0.1");
    const uint_least16_t SERVER_PORT = 12345;
    const int THREADS_NUMBER = 20;
    const std::string LOG_FILE_PATH("/home/dmitry/vscodeprojects/dataleak_parser/server_backend.log");
    const bool CONSOLE_LOG_ENABLED = true;
}

#endif