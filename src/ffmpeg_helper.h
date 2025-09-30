//
// Created by Founder on 2025/9/28.
//

#ifndef RTSP_SERVER_FFMPEG_HELPER_H
#define RTSP_SERVER_FFMPEG_HELPER_H
#include <iostream>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}



inline void rgb2yuv420p(
    const uint8_t* in_data, const size_t data_size,
    const int src_width, const int src_height,
    const int dst_width, const int dst_height,
    const int count) {

    SwsContext* swsCtx = sws_getContext(
        src_width, src_height, AV_PIX_FMT_BGR24,
        dst_width, dst_height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    AVFrame* rgb_frame = av_frame_alloc();
    rgb_frame->format = AV_PIX_FMT_BGR24;
    rgb_frame->width = src_width;
    rgb_frame->height = src_height;
    av_frame_get_buffer(rgb_frame, 32);

    // 填充 rgbFrame->data[0] 为你的 BGR 数据（来自 GetBitmapBitsRGB24）
    memcpy(rgb_frame->data[0], in_data, data_size);

    AVFrame* yuv_frame = av_frame_alloc();
    yuv_frame->format = AV_PIX_FMT_YUV420P;
    yuv_frame->width = dst_width;
    yuv_frame->height = dst_height;
    av_frame_get_buffer(yuv_frame, 32);

    sws_scale(swsCtx, rgb_frame->data, rgb_frame->linesize, 0, src_height, yuv_frame->data, yuv_frame->linesize);
    yuv_frame->pts = count;

}


#endif //RTSP_SERVER_FFMPEG_HELPER_H