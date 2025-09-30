//
// Created by admin on 2025/9/23.
//

#ifndef RTSP_SERVER_MEDIA_SOURCE_H
#define RTSP_SERVER_MEDIA_SOURCE_H

#include <string>

namespace rtsp {
    class Rtsp_session;
}

class Media_source {
public:
    virtual ~Media_source();
    virtual std::string get_sdp() = 0;
    virtual void start_streaming(rtsp::Rtsp_session& session) = 0;
    virtual void stop_streaming() = 0;
};

#endif //RTSP_SERVER_MEDIA_SOURCE_H