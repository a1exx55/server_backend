//local
#include <logging/logger.hpp>
#include <network/server.hpp>

int main()
{
    LOG_INFO << "The server was successfully started!";
    server::run();
    LOG_INFO << "The server was successfully shut down!";
    return 0;
}