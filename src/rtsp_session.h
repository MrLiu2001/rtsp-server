//
// Created by admin on 2025/9/23.
//

#ifndef RTSP_SERVER_RTSP_SESSION_H
#define RTSP_SERVER_RTSP_SESSION_H

#include "socket_util.h"
#include "rtsp_request.h"
#include "rtsp_response.h"
#include "rtp_helper.h"
#include <memory>
#include <vector>


class Media_source;

namespace rtsp {
    class Rtsp_session {
    public:
        Rtsp_session(SOCKET client_socket, std::shared_ptr<Media_source> media_source);
        ~Rtsp_session();
        void run();
        // 新增一个发送接口
        void send_rtp_packet_data(const std::vector<uint8_t> &packet_data);

    private:
        Rtsp_response handle_request(const Rtsp_request& req);
        Rtsp_response do_OPTIONS(const Rtsp_request& req);
        Rtsp_response do_DESCRIBE(const Rtsp_request& req);
        Rtsp_response do_SETUP(const Rtsp_request& req);
        Rtsp_response do_PLAY(const Rtsp_request& req);
        Rtsp_response do_TEARDOWN(const Rtsp_request& req);

        void send_response(const Rtsp_response& resp);

        SOCKET _clientSocket;

        std::shared_ptr<Media_source> _mediaSource;

        enum class State { INIT, READY, PLAYING };

        State _state = State::INIT;

        // RTP/RTCP sockets are now owned by the session
        Rtp_Transport_Type _client_rtp_transport_type = Rtp_Transport_Type::UDP;

        /* UDP */
        sockaddr_in _client_udp_rtp_addr;
        sockaddr_in _client_udp_rtcp_addr;
        SOCKET _server_udp_rtp_socket = INVALID_SOCKET;
        SOCKET _server_udp_rtcp_socket = INVALID_SOCKET;

        /* TCP */
        int _rtp_channel;
        int _rtcp_channel;
    };
} // rtsp

#endif //RTSP_SERVER_RTSP_SESSION_H