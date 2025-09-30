//
// Created by admin on 2025/9/23.
//

#include "rtsp_session.h"
#include "media_source.h"

#include <iostream>
#include <random>

namespace rtsp {
    Rtsp_session::Rtsp_session(SOCKET client_socket, std::shared_ptr<Media_source> media_source)
        : _clientSocket(client_socket), _mediaSource(std::move(media_source)) {

        socklen_t len = sizeof(_client_udp_rtp_addr);
        if (getpeername(_clientSocket, (sockaddr *) &_client_udp_rtp_addr, &len) == SOCKET_ERROR) {
            std::cerr << "getpeername failed with error: " << WSAGetLastError() << std::endl;
        } else {
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &_client_udp_rtp_addr.sin_addr, client_ip, sizeof(client_ip));
            std::cout << "Session _client_udp_rtp_addr created for client at: " << client_ip << std::endl;
        }

        socklen_t len2 = sizeof(_client_udp_rtcp_addr);
        if (getpeername(_clientSocket, (sockaddr *) &_client_udp_rtcp_addr, &len2) == SOCKET_ERROR) {
            std::cerr << "getpeername failed with error: " << WSAGetLastError() << std::endl;
        } else {
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &_client_udp_rtcp_addr.sin_addr, client_ip, sizeof(client_ip));
            std::cout << "Session _client_udp_rtcp_addr created for client at: " << client_ip << std::endl;
        }

    }

    Rtsp_session::~Rtsp_session() {
        if (_state == State::PLAYING) {
            _mediaSource->stop_streaming();
        }
        closesocket(_clientSocket);
        if (_server_udp_rtp_socket != INVALID_SOCKET) closesocket(_server_udp_rtp_socket);
        if (_server_udp_rtcp_socket != INVALID_SOCKET) closesocket(_server_udp_rtcp_socket);
        std::cout << "Session destroyed." << std::endl;
    }

    void Rtsp_session::run() {
        char buffer[2048];
        while (true) {
            const int bytes_received = recv(_clientSocket, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0) {
                std::cout << "Client disconnected." << std::endl;
                break;
            }
            buffer[bytes_received] = '\0';
            std::cout << "--- C->S ---\n" << buffer << "------------\n";

            Rtsp_request req = Rtsp_request::parse(std::string(buffer));
            Rtsp_response resp = handle_request(req);
            send_response(resp);

            if (req.method() == Rtsp_request::TEARDOWN) {
                break; // Close session after TEARDOWN
            }
        }
    }

    void Rtsp_session::send_rtp_packet_data(const std::vector<uint8_t> &packet_data) {
        if (_client_rtp_transport_type == Rtp_Transport_Type::TCP) {
            // **关键：封装交错头**
            uint16_t data_size = packet_data.size();
            std::vector<uint8_t> interleaved_frame;
            interleaved_frame.reserve(4 + data_size);

            interleaved_frame.push_back('$');
            interleaved_frame.push_back(_rtp_channel); // 使用在SETUP时保存的通道号
            interleaved_frame.push_back((data_size >> 8) & 0xFF); // 长度高字节
            interleaved_frame.push_back(data_size & 0xFF);        // 长度低字节

            // 附加RTP数据
            interleaved_frame.insert(interleaved_frame.end(), packet_data.begin(), packet_data.end());
            send(_clientSocket, reinterpret_cast<const char *>(interleaved_frame.data()), interleaved_frame.size(), 0);

        } else {
            sendto(_server_udp_rtp_socket, reinterpret_cast<const char *>(packet_data.data()),
                                packet_data.size(), 0,
                                reinterpret_cast<sockaddr *>(&_client_udp_rtp_addr), sizeof(_client_udp_rtp_addr));
        }

    }

    void Rtsp_session::send_response(const Rtsp_response &resp) {
        std::string resp_str = resp.to_string();
        std::cout << "--- S->C ---\n" << resp_str << "------------\n";
        send(_clientSocket, resp_str.c_str(), resp_str.length(), 0);
    }

    Rtsp_response Rtsp_session::handle_request(const Rtsp_request &req) {
        switch (req.method()) {
            case Rtsp_request::OPTIONS: return do_OPTIONS(req);
            case Rtsp_request::DESCRIBE: return do_DESCRIBE(req);
            case Rtsp_request::SETUP: return do_SETUP(req);
            case Rtsp_request::PLAY: return do_PLAY(req);
            case Rtsp_request::TEARDOWN: return do_TEARDOWN(req);
            default: return Rtsp_response(400, "Bad Request");
        }
    }

    Rtsp_response Rtsp_session::do_OPTIONS(const Rtsp_request &req) {
        Rtsp_response resp(200, "OK");
        resp.add_header("CSeq", std::to_string(req.cseq()));
        resp.add_header("Public", "DESCRIBE, SETUP, TEARDOWN, PLAY, OPTIONS");
        return resp;
    }

    Rtsp_response Rtsp_session::do_DESCRIBE(const Rtsp_request &req) {
        std::string sdp = _mediaSource->get_sdp();
        if (sdp.empty()) {
            return Rtsp_response(404, "Not Found");
        }
        Rtsp_response resp(200, "OK");
        resp.add_header("CSeq", std::to_string(req.cseq()));
        resp.add_header("Content-Type", "application/sdp");
        resp.set_body(sdp);
        return resp;
    }

    Rtsp_response Rtsp_session::do_SETUP(const Rtsp_request &req) {
        // Parse transport header to get client ports
        std::string transport = req.get_header("Transport");
        // 检查是否是交错模式
        const size_t interleaved_pos = transport.find("interleaved=");
        if (interleaved_pos != std::string::npos) {
            // 是交错模式
            _client_rtp_transport_type = Rtp_Transport_Type::TCP;
            // **解析并保存通道号**
            std::string interleaved_str = transport.substr(interleaved_pos + 12);
            _rtp_channel = std::stoi(interleaved_str.substr(0, interleaved_str.find('-')));
            _rtcp_channel = _rtp_channel + 1; // 通常是连续的

            Rtsp_response resp(200, "OK");
            resp.add_header("CSeq", std::to_string(req.cseq()));
            resp.add_header("Transport", transport);
            resp.add_header("Session", "12345678");
            _state = State::READY;
            return resp;
        }

        const size_t port_pos = transport.find("client_port=");
        std::string port_str = transport.substr(port_pos + 12);
        const int client_rtp_port = std::stoi(port_str.substr(0, port_str.find('-')));


        _client_udp_rtp_addr.sin_port = htons(client_rtp_port);
        _client_udp_rtcp_addr.sin_port = htons(client_rtp_port + 1);

        // Create and bind server sockets
        _server_udp_rtp_socket = socket(AF_INET, SOCK_DGRAM, 0);
        _server_udp_rtcp_socket = socket(AF_INET, SOCK_DGRAM, 0);

        if (_server_udp_rtp_socket == INVALID_SOCKET || _server_udp_rtcp_socket == INVALID_SOCKET) {
            // 错误处理：无法创建socket
            return Rtsp_response(500, "Can not create socket");
        }

        // 2. 准备绑定地址结构，并请求操作系统分配端口
        sockaddr_in server_rtp_addr{};
        server_rtp_addr.sin_family = AF_INET;
        server_rtp_addr.sin_addr.s_addr = INADDR_ANY; // 监听所有网卡
        server_rtp_addr.sin_port = htons(0); // **关键：将端口设置为0，表示让操作系统自动选择一个可用端口**

        for (int i = 0; i < 10; ++i) { // 尝试10次

            // 3a. 绑定RTP套接字
            if (bind(_server_udp_rtp_socket, reinterpret_cast<sockaddr *>(&server_rtp_addr), sizeof(server_rtp_addr)) == SOCKET_ERROR) {
                // 绑定失败，可能是端口被占用，虽然我们请求的是随机端口，但以防万一
                closesocket(_server_udp_rtp_socket);
                closesocket(_server_udp_rtcp_socket);
                // 错误处理
                return Rtsp_response(500, "Can not bind socket");
            }

            // 3b. **查询操作系统为RTP分配的端口号**
            sockaddr_in bound_rtp_addr{};
            int len = sizeof(bound_rtp_addr);
            if (getsockname(_server_udp_rtp_socket, reinterpret_cast<sockaddr *>(&bound_rtp_addr), &len) == SOCKET_ERROR) {
                // 查询失败
                return Rtsp_response(500, "Can not get socket name");
            }
            int server_rtp_port = ntohs(bound_rtp_addr.sin_port);

            // 3c. 确保RTP端口是偶数 (协议建议)
            if (server_rtp_port % 2 != 0) {
                // 如果不是偶数，关闭socket，重新尝试
                closesocket(_server_udp_rtp_socket);
                _server_udp_rtp_socket = socket(AF_INET, SOCK_DGRAM, 0); // 重新创建
                continue; // 进入下一次循环
            }

            // 3d. 尝试绑定RTCP到下一个端口 (RTP端口 + 1)
            sockaddr_in server_rtcp_addr = server_rtp_addr;
            server_rtcp_addr.sin_port = htons(server_rtp_port + 1);

            if (bind(_server_udp_rtcp_socket, reinterpret_cast<sockaddr *>(&server_rtcp_addr), sizeof(server_rtcp_addr)) == SOCKET_ERROR) {
                // RTCP端口绑定失败（可能被其他程序占用了）
                // 关闭两个socket，重新尝试获取一对新的端口
                closesocket(_server_udp_rtp_socket);
                closesocket(_server_udp_rtcp_socket);
                _server_udp_rtp_socket = socket(AF_INET, SOCK_DGRAM, 0);
                _server_udp_rtcp_socket = socket(AF_INET, SOCK_DGRAM, 0);
                continue; // 进入下一次循环
            }
            // 4. 成功
            int server_rtcp_port = server_rtp_port + 1;

            std::cout << "Server ports allocated: RTP=" << server_rtp_port << ", RTCP=" << server_rtcp_port << std::endl;

            _client_rtp_transport_type = Rtp_Transport_Type::UDP; // 设置会话模式
            // 保存客户端地址等其他信息
            Rtsp_response resp(200, "OK");
            resp.add_header("CSeq", std::to_string(req.cseq()));
            const std::string transport_header = "RTP/AVP;unicast;client_port=" + std::to_string(client_rtp_port) + "-" +
                                                 std::to_string(client_rtp_port + 1)
                                                 + ";server_port=" + std::to_string(server_rtp_port) + "-" + std::to_string(
                                                     server_rtcp_port);
            resp.add_header("Transport", transport_header);
            resp.add_header("Session", "12345678"); // Use a proper session ID
            _state = State::READY;
            return resp;
        }

        return Rtsp_response(500, "Unknown error");
    }

    Rtsp_response Rtsp_session::do_PLAY(const Rtsp_request &req) {
        if (_state != State::READY) {
            return Rtsp_response(405, "Method Not Allowed");
        }

        _mediaSource->start_streaming(*this);

        Rtsp_response resp(200, "OK");
        resp.add_header("CSeq", std::to_string(req.cseq()));
        resp.add_header("Session", "12345678");
        resp.add_header("Range", "npt=0.000-");
        _state = State::PLAYING;
        return resp;
    }

    Rtsp_response Rtsp_session::do_TEARDOWN(const Rtsp_request &req) {
        if (_state == State::PLAYING) {
            _mediaSource->stop_streaming();
        }
        Rtsp_response resp(200, "OK");
        resp.add_header("CSeq", std::to_string(req.cseq()));
        _state = State::INIT;
        return resp;
    }
} // rtsp
