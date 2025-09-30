//
// Created by Founder on 2025/9/27.
//

#ifndef RTSP_SERVER_H264_STREAM_MEDIA_SOURCE_H
#define RTSP_SERVER_H264_STREAM_MEDIA_SOURCE_H
#include <atomic>
#include <deque>
#include <thread>
#include <vector>
#include <winsock2.h>
#include <windows.h>
#include <gdiplus.h>
#include <iostream>

#include "media_source.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace rtsp_source {
    class win10_h264_stream_media_source : public Media_source {
    public:
        explicit win10_h264_stream_media_source();
        ~win10_h264_stream_media_source() override;

        std::string get_sdp() override;
        void start_streaming(rtsp::Rtsp_session &session) override;
        void stop_streaming() override;

    private:
        void streaming_thread_func();
        std::vector<uint8_t> capture_screen() const;

        bool encode_frame_and_send(AVCodecContext *enc_ctx, const AVFrame *frame, uint16_t &seq_num);


        std::deque<AVPacket> _stream_buffer;
        std::thread _streamingThread;
        std::atomic<bool> _isStreaming;

        rtsp::Rtsp_session* _session;

        AVFormatContext* _format_ctx = nullptr;
        AVCodecContext* _codec_ctx = nullptr;
        SwsContext* _sws_ctx = nullptr;
        AVFrame* _yuv_frame = nullptr;
        int _fps = 30;
        int _count = 0;

        int _screen_width, _screen_height;
        int _width, _height;
    };
}


#endif //RTSP_SERVER_H264_STREAM_MEDIA_SOURCE_H