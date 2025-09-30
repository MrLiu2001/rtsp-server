//
// Created by admin on 2025/9/23.
//

#include "h264_file_media_source.h"
#include "rtp_packet.h" // Include the RtpPacket.h from previous example
#include <fstream>
#include <iostream>
#include <utility>
#include <vector>
#include <chrono>
#include <sstream>

#include "rtsp_session.h"

namespace rtsp_source {

    h264_file_media_source::h264_file_media_source(std::string file_path)
        : _filePath(std::move(file_path)), _isStreaming(false), _session(nullptr) {
    }

    h264_file_media_source::~h264_file_media_source() {
        h264_file_media_source::stop_streaming();
    }

    std::string h264_file_media_source::get_sdp() {
        // Same SDP generation logic as before
        // You MUST replace these with the correct values for YOUR video file.
        const std::string sps_pps = "J2QAM6wTGlgUAW7AWoCAgKAAAAMAIAAAB4HixJCw,KO4CvLA=;";

        std::ostringstream sdp;
        sdp << "v=0\r\n"
                << "o=- 0 0 IN IP4 127.0.0.1\r\n"
                << "s=H.264 File Stream\r\n"
                << "t=0 0\r\n"
                << "m=video 9900 RTP/AVP 96\r\n"
                << "a=framerate:30\r\n"
                << "a=tool:libavformat 62.6.100\r\n"
                << "a=rtpmap:96 H264/90000\r\n"
                << "a=fmtp:96 packetization-mode=1; sprop-parameter-sets=" << sps_pps << "; profile-level-id=640033\r\n"
                << "a=control:trackID=0\r\n";
        return sdp.str();
    }

    void h264_file_media_source::start_streaming(rtsp::Rtsp_session &session) {
        if (_isStreaming) {
            return; // Already streaming
        }
        _session = &session;
        _isStreaming = true;
        _streamingThread = std::thread(&h264_file_media_source::streaming_thread_func, this);
    }

    void h264_file_media_source::stop_streaming() {
        _isStreaming = false;
        if (_streamingThread.joinable()) {
            _streamingThread.join();
        }
    }

    // The entire rtp_streaming_thread logic goes here, renamed to streaming_thread_func
    // and using m_params to get socket/client info.
    void h264_file_media_source::streaming_thread_func() {
        std::cout << "Streaming thread started for file: " << _filePath << std::endl;

        // 1. Read the entire H.264 file into memory
        std::ifstream file(_filePath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Could not open H.264 file: " << _filePath << std::endl;
            return;
        }

        std::vector<uint8_t> file_buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        if (file_buffer.empty()) {
            std::cerr << "H.264 file is empty or could not be read." << std::endl;
            return;
        }

        // 3. Initialize RTP parameters
        uint16_t seqNum = 0;
        uint32_t timestamp = 0;
        uint32_t ssrc = 12345; // Should be a random number, consistent for the session
        const int clockRate = 90000;
        const int frameRate = 25; // Assume 25 FPS for this file
        const int timestampIncrement = clockRate / frameRate;
        const int MTU = 1400; // Maximum Transmission Unit for RTP payload

        // 4. Start the NALU parsing and streaming loop
        const uint8_t *buffer_ptr = file_buffer.data();
        const uint8_t *buffer_end = file_buffer.data() + file_buffer.size();
        const uint8_t *nalu_start = find_start_code(buffer_ptr, buffer_end);

        while (_isStreaming && nalu_start < buffer_end) {
            const uint8_t *nalu_end = find_start_code(nalu_start, buffer_end);
            size_t nalu_size = nalu_end - nalu_start;
            // Adjust for the next search start point if it's the end of the buffer
            if (nalu_end == buffer_end) {
                nalu_size = buffer_end - nalu_start;
            }

            if (nalu_size > 0) {
                uint8_t nalu_header = *nalu_start;
                uint8_t nalu_type = nalu_header & 0x1F;

                // Skip SPS/PPS as they are usually sent via SDP
                // 如果sdp信息包含了sps和pps，就可以跳过对sps和pps帧的发送
                if (nalu_type == NALU_TYPE_SPS || nalu_type == NALU_TYPE_PPS) {
                    nalu_start = nalu_end;
                    continue;
                }

                RtpPacket packet;
                packet.header.version = 2;
                packet.header.padding = 0;
                packet.header.extension = 0;
                packet.header.csrc_len = 0;
                packet.header.payload_type = 96; // Dynamic payload type for H.264
                packet.header.ssrc = ssrc;
                packet.header.timestamp = timestamp;

                // Marker bit (M) should be 1 for the last packet of a frame
                bool is_last_packet_of_frame = true;

                // Case A: Single NAL Unit Packet (NALU fits in one RTP packet)
                if (nalu_size <= MTU) {
                    packet.header.marker = 1; // The single packet is also the last packet
                    packet.header.seq_num = seqNum++;
                    packet.payload.assign(nalu_start, nalu_start + nalu_size);

                    auto packet_bytes = packet.to_bytes();
                    _session->send_rtp_packet_data(packet_bytes);
                }
                // Case B: Fragmentation Unit (FU-A) (NALU must be fragmented)
                else {
                    is_last_packet_of_frame = false;

                    uint8_t fu_indicator = (nalu_header & 0xE0) | NALU_TYPE_FU_A; // Keep NRI, set type to FU-A

                    const uint8_t *nalu_data_ptr = nalu_start + 1; // Skip the original NALU header
                    size_t remaining_size = nalu_size - 1;

                    bool is_first_fragment = true;

                    while (remaining_size > 0) {
                        size_t fragment_size = std::min(remaining_size, (size_t) (MTU - 2));
                        // 2 bytes for FU indicator and header

                        packet.header.seq_num = seqNum++;
                        packet.payload.clear();
                        packet.payload.push_back(fu_indicator);

                        // FU Header
                        uint8_t fu_header = nalu_type;
                        bool is_last_fragment = (remaining_size == fragment_size);
                        if (is_first_fragment) {
                            fu_header |= 0x80; // Start bit
                            is_first_fragment = false;
                        }
                        if (is_last_fragment) {
                            fu_header |= 0x40; // End bit
                            is_last_packet_of_frame = true; // This fragment is the end of the frame
                        }
                        packet.payload.push_back(fu_header);

                        packet.payload.insert(packet.payload.end(), nalu_data_ptr, nalu_data_ptr + fragment_size);

                        packet.header.marker = is_last_fragment ? 1 : 0;

                        auto packet_bytes = packet.to_bytes();
                        _session->send_rtp_packet_data(packet_bytes);

                        nalu_data_ptr += fragment_size;
                        remaining_size -= fragment_size;
                    }
                }

                // Only increment timestamp after a full frame has been sent
                if (is_last_packet_of_frame) {
                    timestamp += timestampIncrement;
                }
            }

            nalu_start = nalu_end;

            // Sleep to simulate the frame rate
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 / frameRate));
        }

        std::cout << "Streaming thread finished." << std::endl;
    } // rtsp_source
}
