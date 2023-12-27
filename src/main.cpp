//local
#include <network/server.hpp>

int main()
{
    // Run all server logic and wait until either manual shutdown via SIGINT, SIGTERM signals 
    // or unexpected error followed by program termination after resources cleanup 
    server::run();

    return 0;
}