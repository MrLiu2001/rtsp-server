//
// Created by admin on 2025/9/23.
//

#ifndef RTSP_SERVER_SOCKET_UTIL_H
#define RTSP_SERVER_SOCKET_UTIL_H

#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

inline bool initialize_sockets() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return false;
    }
#endif
    return true;
}

inline void cleanup_sockets() {
#ifdef _WIN32
    WSACleanup();
#endif
}


#endif //RTSP_SERVER_SOCKET_UTIL_H
