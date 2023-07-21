//local
#include <logging/logger.hpp>
#include <network/server.hpp>

int main()
{
    server::run();
    LOG_INFO << "Server was successfully shut down!";
    return 0;
}