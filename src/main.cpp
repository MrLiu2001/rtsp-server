#include "socket_util.h"
#include "rtsp_server.h"

int main() {
    std::cout << "Starting RTSP Server..." << std::endl;
    if (!initialize_sockets()) {
        return 1;
    }

    rtsp::Rtsp_server server(8554);
    server.start(); // This will block until the server is stopped

    cleanup_sockets();
    char c;
    std::cin >> c;
    return 0;
}