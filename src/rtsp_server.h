//
// Created by admin on 2025/9/23.
//

#ifndef RTSP_SERVER_RTSP_SERVER_H
#define RTSP_SERVER_RTSP_SERVER_H

#include "socket_util.h"
#include <vector>
#include <thread>
#include <atomic>


namespace rtsp {
    class Rtsp_server {
    public:
        explicit Rtsp_server(int port);

        ~Rtsp_server();

        void start();
        void stop();

    private:
        void accept_connections();

        int _port;
        SOCKET _listenSocket;
        std::atomic<bool> _isRunning;
        std::thread _acceptThread;
        std::vector<std::thread> _clientThreads;


    };
} // rtsp

#endif //RTSP_SERVER_RTSP_SERVER_H