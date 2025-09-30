//
// Created by admin on 2025/9/23.
//

#include "rtsp_server.h"
#include "rtsp_session.h"
#include <iostream>

#include "win10_h264_stream_media_source.h"

namespace rtsp {
    Rtsp_server::Rtsp_server(int port) : _port(port), _listenSocket(INVALID_SOCKET), _isRunning(false) {
    }

    Rtsp_server::~Rtsp_server() {
        stop();
    }

    void Rtsp_server::start() {
        _listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (_listenSocket == INVALID_SOCKET) {
            std::cerr << "Error creating socket.\n";
            return;
        }

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(_port);

        if (bind(_listenSocket, reinterpret_cast<sockaddr *>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "Bind failed.\n";
            closesocket(_listenSocket);
            return;
        }

        if (listen(_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "Listen failed.\n";
            closesocket(_listenSocket);
            return;
        }

        _isRunning = true;
        _acceptThread = std::thread(&Rtsp_server::accept_connections, this);
        std::cout << "RTSP Server listening on port " << _port << std::endl;
        _acceptThread.join(); // Wait here until server stops
    }

    void Rtsp_server::stop() {
        _isRunning = false;
        if (_listenSocket != INVALID_SOCKET) {
            closesocket(_listenSocket);
            _listenSocket = INVALID_SOCKET;
        }
        if (_acceptThread.joinable()) {
            _acceptThread.join();
        }
        for (auto &th: _clientThreads) {
            if (th.joinable()) {
                th.join();
            }
        }
    }

    void Rtsp_server::accept_connections() {
        while (_isRunning) {
            SOCKET clientSocket = accept(_listenSocket, nullptr, nullptr);
            if (clientSocket == INVALID_SOCKET) {
                if (_isRunning) {
                    std::cerr << "Accept failed.\n";
                }
                break; // Socket was closed, exit loop
            }

            std::cout << "Accepted new connection." << std::endl;

            _clientThreads.emplace_back([clientSocket]() {
                // For each session, create a media source.
                // In a real server, you'd have a media source manager
                // that maps a URL to a source.
                auto media_source = std::make_shared<rtsp_source::win10_h264_stream_media_source>();
                Rtsp_session session(clientSocket, media_source);
                session.run();
            });
        }
    }
} // rtsp