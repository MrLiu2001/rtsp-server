//
// Created by admin on 2025/9/23.
//

#ifndef RTSP_SERVER_H264_MEDIA_SOURCE_H
#define RTSP_SERVER_H264_MEDIA_SOURCE_H

#include "media_source.h"
#include <thread>
#include <atomic>
#include <string>

namespace rtsp_source {
    class h264_file_media_source : public Media_source {
    public:
        explicit h264_file_media_source(std::string  file_path);
        ~h264_file_media_source() override;

        std::string get_sdp() override;
        void start_streaming(rtsp::Rtsp_session &session) override;
        void stop_streaming() override;

    private:
        void streaming_thread_func();

        std::string _filePath;
        std::thread _streamingThread;
        std::atomic<bool> _isStreaming;

        rtsp::Rtsp_session* _session;
    };
} // rtsp_source

#endif //RTSP_SERVER_H264_MEDIA_SOURCE_H