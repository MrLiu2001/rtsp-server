//
// Created by Founder on 2025/9/27.
//

#include "win10_h264_stream_media_source.h"

#include "rtp_packet.h"
#include "rtp_helper.h"
#include "rtsp_session.h"

namespace rtsp_source {
    win10_h264_stream_media_source::win10_h264_stream_media_source()
        : _isStreaming(false), _session(nullptr) {
        // 获取屏幕分辨率
        _screen_width = GetSystemMetrics(SM_CXSCREEN);
        _screen_height = GetSystemMetrics(SM_CYSCREEN);

        _width = (_screen_width / 2) / 2 * 2; // 先除以2，再乘以2，确保是偶数
        _height = (_screen_height / 2) / 2 * 2; // 同上

        const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!codec) {
            std::cerr << "Codec not found\n";
            return;
        }

        AVCodecContext *ctx = avcodec_alloc_context3(codec);

        if (!ctx) {
            std::cerr << "Could not allocate video codec context\n";
            return;
        }

        ctx->bit_rate = 2000000;
        ctx->width = _width;
        ctx->height = _height;
        ctx->time_base = {1, _fps};
        ctx->framerate = {_fps, 1};
        ctx->gop_size = _fps * 2;
        ctx->max_b_frames = 0;
        ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        // 设置libx264的特定选项，例如预设和调优
        av_opt_set(ctx->priv_data, "preset", "veryfast", 0);
        av_opt_set(ctx->priv_data, "tune", "zerolatency", 0); // 适合实时录屏

        if (avcodec_open2(ctx, codec, nullptr) < 0) {
            std::cerr << "Could not open codec\n";
            return;
        }
        _codec_ctx = ctx;

        //  准备数据转换 (BGRA -> YUV420P)
        _sws_ctx = sws_getContext(
            _screen_width, _screen_height, AV_PIX_FMT_BGRA, // 输入
            _width, _height, AV_PIX_FMT_YUV420P, // 输出
            SWS_BILINEAR, nullptr, nullptr, nullptr);

        if (!_sws_ctx) {
            std::cerr << "Fail to create _sws_ctx!";
        }

        _yuv_frame = av_frame_alloc();
        _yuv_frame->format = AV_PIX_FMT_YUV420P;
        _yuv_frame->width = _width;
        _yuv_frame->height = _height;

        const int ret = av_frame_get_buffer(_yuv_frame, 0); // 第二个参数 align 通常为0
        if (ret < 0) {
            std::cout << "av_frame_get_buffer failed!";
            // Handle error: Allocation failed!
            // This is the most likely source of your problem.
        }
    }

    win10_h264_stream_media_source::~win10_h264_stream_media_source() {
        win10_h264_stream_media_source::stop_streaming();

        if (_format_ctx) {
            avformat_free_context(_format_ctx);
        }
        if (_codec_ctx) {
            avcodec_free_context(&_codec_ctx);
        }
        if (_yuv_frame) {
            av_frame_free(&_yuv_frame);
        }
        if (_sws_ctx) {
            sws_freeContext(_sws_ctx);
        }
    }


    void win10_h264_stream_media_source::start_streaming(rtsp::Rtsp_session &session) {
        if (_isStreaming) {
            return; // Already streaming
        }
        _session = &session;
        _isStreaming = true;
        //streaming_thread_func();
        _streamingThread = std::thread(&win10_h264_stream_media_source::streaming_thread_func, this);
    }

    void win10_h264_stream_media_source::stop_streaming() {
        _isStreaming = false;

        if (_streamingThread.joinable()) {
            _streamingThread.join();
        }
    }

    std::string win10_h264_stream_media_source::get_sdp() {
        // *** 核心部分：使用RTP Muxer生成SDP ***
        const char *dummy_rtp_url = "rtp://127.0.0.1:65500"; // 地址不重要
        avformat_alloc_output_context2(&_format_ctx, nullptr, "rtp", dummy_rtp_url);
        if (!_format_ctx) throw std::runtime_error("Could not create output context for SDP generation");

        AVStream *stream = avformat_new_stream(_format_ctx, nullptr);
        if (!stream) throw std::runtime_error("Failed allocating output stream");

        //stream->id = _format_ctx->nb_streams - 1;
        //stream->time_base = _format_ctx->streams.;
        const auto ret = avcodec_parameters_from_context(stream->codecpar, _codec_ctx);

        if (ret < 0) {
            char err_buf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, err_buf, sizeof(err_buf));
            throw std::runtime_error("Failed to copy codec parameters: " + std::string(err_buf));
        }

        // **严格检查**：确保参数复制后，codec_id 等关键字段是正确的
        if (stream->codecpar->codec_id == AV_CODEC_ID_NONE) {
            throw std::runtime_error("Codec parameters were not copied correctly.");
        }

        //avcodec_open2(_ctx, code, nullptr);
        //av_dump_format(_format_ctx, 0, dummy_rtp_url, 1); // 打印上下文信息，非常有用的调试工具

        // 写入头信息（但不会真地发送），这一步会填充生成SDP所需的信息
        //auto res = avformat_write_header(_format_ctx, NULL);

        // if (res < 0) {
        //     throw std::runtime_error("Failed to avformat_write_header ");
        // }

        // 提取SDP信息
        char sdp_buffer[4096];
        av_sdp_create(&_format_ctx, 1, sdp_buffer, sizeof(sdp_buffer));

        return std::string(sdp_buffer);
    }

    void win10_h264_stream_media_source::streaming_thread_func() {
        uint16_t seq_num = 0;
        using ms = std::chrono::milliseconds;
        auto frame_interval = ms(1000 / _fps);
        auto next_frame_time = std::chrono::high_resolution_clock::now();

        while (_isStreaming) {
            auto screen = capture_screen();
            const int in_line_size[1] = {_screen_width * 4};
            const uint8_t *const in_data[1] = {screen.data()};

            // 执行颜色转换
            // 记录开始时间
            auto start = std::chrono::high_resolution_clock::now();
            sws_scale(_sws_ctx, in_data, in_line_size, 0, _screen_height, _yuv_frame->data, _yuv_frame->linesize);
            _yuv_frame->pts = _count++;

            // 编码这一帧
            encode_frame_and_send(_codec_ctx, _yuv_frame, seq_num);

            // 记录结束时间
            auto end = std::chrono::high_resolution_clock::now();
            // 计算时间差
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            //std::cout << "Encoded frame " << _count << ", duration: " << duration.count() << "ms\r\n";
            // **新的、精确的睡眠逻辑**
            next_frame_time += frame_interval;
            auto sleep_duration = std::chrono::duration_cast<ms>(next_frame_time - std::chrono::high_resolution_clock::now());

            if (sleep_duration.count() > 0) {
                std::this_thread::sleep_for(sleep_duration);
            } else {
                // 如果处理时间已经超过了帧间隔，我们可能需要调整下一次的时间点
                // 或者简单地立即进入下一帧
                if (std::chrono::high_resolution_clock::now() > next_frame_time + frame_interval) {
                    // 如果我们已经落后了不止一帧，重置时间点以避免追赶
                    next_frame_time = std::chrono::high_resolution_clock::now();
                }
            }
        }
    }

    std::vector<uint8_t> win10_h264_stream_media_source::capture_screen() const {
        HDC hScreenDC = GetDC(nullptr);
        HDC hMemoryDC = CreateCompatibleDC(hScreenDC);

        HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, _screen_width, _screen_height);
        auto hOldBitmap = static_cast<HBITMAP>(SelectObject(hMemoryDC, hBitmap));

        // 从屏幕拷贝到内存DC
        auto err = BitBlt(hMemoryDC, 0, 0, _screen_width, _screen_height, hScreenDC, 0, 0, SRCCOPY);

        if (!err) {
            const DWORD error = GetLastError();
            std::cout << error << std::endl;
        }

        hBitmap = static_cast<HBITMAP>(SelectObject(hMemoryDC, hOldBitmap));

        std::vector<uint8_t> buffer(_screen_width * _screen_height * 4); // 32-bit BGRA

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = _screen_width;
        bmi.bmiHeader.biHeight = -_screen_height; // 负数表示顶-底DIB
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        GetDIBits(hMemoryDC, hBitmap, 0, _screen_height, buffer.data(), &bmi, DIB_RGB_COLORS);

        // 清理GDI资源
        DeleteObject(hBitmap);
        DeleteDC(hMemoryDC);
        ReleaseDC(nullptr, hScreenDC);

        return buffer;
    }

    bool win10_h264_stream_media_source::encode_frame_and_send(AVCodecContext *enc_ctx, const AVFrame *frame,
                                                               uint16_t &seq_num) {
        if (!_session) return false;

        int ret = avcodec_send_frame(enc_ctx, frame);
        if (ret < 0) return false;

        AVPacket *pkt = av_packet_alloc();
        if (!pkt) return false;

        const int MTU = 1400;
        const int clockRate = 90000;
        const int frameRate = _fps;

        // 接收编码后的包，一个帧可能产生多个包
        while (true) {
            ret = avcodec_receive_packet(enc_ctx, pkt);
            if (ret < 0) return false; // 包括 EAGAIN 和 EOF

            const uint8_t *data = pkt->data;
            const uint8_t *buffer_end = data + pkt->size;
            const uint8_t *current_pos = data;

            uint32_t rtp_timestamp = pkt->pts * clockRate / frameRate;

            while (current_pos < buffer_end) {
                const uint8_t *start_code_ptr = find_start_code_boundary(current_pos, buffer_end);
                if (!start_code_ptr) break;

                const int start_code_len = (start_code_ptr[2] == 1) ? 3 : 4;
                const uint8_t *nalu_start = start_code_ptr + start_code_len;

                const uint8_t *next_start_code_ptr = find_start_code_boundary(nalu_start, buffer_end);

                const uint8_t *nalu_end = (next_start_code_ptr) ? next_start_code_ptr : buffer_end;
                const size_t nalu_size = nalu_end - nalu_start;

                const bool is_last_nalu_of_packet = (next_start_code_ptr == nullptr);

                if (nalu_size > 0) {
                    const uint8_t nalu_header = *nalu_start;
                    const uint8_t nalu_type = nalu_header & 0x1F;

                    if (nalu_type == 7 || nalu_type == 8) {
                        // SPS or PPS
                        current_pos = nalu_end;
                        continue; // 跳过
                    }

                    RtpPacket packet;
                    packet.header.version = 2;
                    packet.header.padding = 0;
                    packet.header.extension = 0;
                    packet.header.csrc_len = 0;
                    packet.header.payload_type = 96; // Dynamic payload type for H.264
                    packet.header.ssrc = 123456;
                    packet.header.timestamp = rtp_timestamp;

                    // 单包模式
                    if (nalu_size <= MTU) {
                        packet.header.marker = is_last_nalu_of_packet ? 1 : 0;
                        packet.header.seq_num = seq_num++;
                        packet.payload.assign(nalu_start, nalu_start + nalu_size);
                        _session->send_rtp_packet_data(packet.to_bytes());
                    }
                    // FU-A 分片模式
                    else {
                        uint8_t fu_indicator = (nalu_header & 0xE0) | 28;
                        const uint8_t *nalu_data_ptr = nalu_start + 1;
                        size_t remaining_size = nalu_size - 1;

                        bool is_first_fragment = true;
                        while (remaining_size > 0) {
                            const size_t fragment_size = std::min(remaining_size, static_cast<size_t>(MTU - 2));
                            const bool is_last_fragment = (remaining_size <= fragment_size);

                            packet.header.marker = (is_last_fragment && is_last_nalu_of_packet) ? 1 : 0;
                            packet.header.seq_num = seq_num++;

                            packet.payload.clear();
                            packet.payload.push_back(fu_indicator);

                            uint8_t fu_header = nalu_type;
                            if (is_first_fragment) fu_header |= 0x80;
                            if (is_last_fragment) fu_header |= 0x40;
                            packet.payload.push_back(fu_header);

                            packet.payload.insert(packet.payload.end(), nalu_data_ptr, nalu_data_ptr + fragment_size);

                            _session->send_rtp_packet_data(packet.to_bytes());

                            nalu_data_ptr += fragment_size;
                            remaining_size -= fragment_size;
                            is_first_fragment = false;
                        }
                    }
                }
                current_pos = nalu_end;
            }
            av_packet_unref(pkt);
        }

        av_packet_free(&pkt);
    }
}
