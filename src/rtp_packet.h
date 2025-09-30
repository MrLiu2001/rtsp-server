//
// Created by admin on 2025/9/23.
//

#ifndef RTSP_SERVER_RTP_PACKET_H
#define RTSP_SERVER_RTP_PACKET_H

#include <cstdint>
#include <vector>
#include <cstring> // For memcpy

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

// H.264 NALU类型
enum NaluType {
    NALU_TYPE_SLICE = 1,
    NALU_TYPE_DPA = 2,
    NALU_TYPE_DPB = 3,
    NALU_TYPE_DPC = 4,
    NALU_TYPE_IDR = 5,
    NALU_TYPE_SEI = 6,
    NALU_TYPE_SPS = 7,
    NALU_TYPE_PPS = 8,
    NALU_TYPE_AUD = 9,
    NALU_TYPE_EOSEQ = 10,
    NALU_TYPE_EOSTREAM = 11,
    NALU_TYPE_FILL = 12,
    NALU_TYPE_FU_A = 28
};

// RTP头部结构
struct RtpHeader {
    // 字节 0
    uint8_t csrc_len : 4;
    uint8_t extension : 1;
    uint8_t padding : 1;
    uint8_t version : 2;
    // 字节 1
    uint8_t payload_type : 7;
    uint8_t marker : 1;
    // 字节 2-3
    uint16_t seq_num;
    // 字节 4-7
    uint32_t timestamp;
    // 字节 8-11
    uint32_t ssrc;
};

struct RtpPacket {
    RtpHeader header;
    std::vector<uint8_t> payload;

    std::vector<uint8_t> to_bytes() const {
        // Total size is 12 bytes for header + payload size
        std::vector<uint8_t> buffer(12 + payload.size());

        // Fill the header fields
        buffer[0] = (header.version << 6) | (header.padding << 5) | (header.extension << 4) | header.csrc_len;
        buffer[1] = (header.marker << 7) | header.payload_type;

        // Use htons and htonl for multi-byte fields to ensure network byte order
        const uint16_t seq_num_n = htons(header.seq_num);
        memcpy(&buffer[2], &seq_num_n, 2);

        const uint32_t timestamp_n = htonl(header.timestamp);
        memcpy(&buffer[4], &timestamp_n, 4);

        const uint32_t ssrc_n = htonl(header.ssrc);
        memcpy(&buffer[8], &ssrc_n, 4);

        // Copy payload data
        if (!payload.empty()) {
            memcpy(buffer.data() + 12, payload.data(), payload.size());
        }

        return buffer;
    }
};

// FU-A Fragmentation Unit Header
struct FuHeader {
    uint8_t type : 5;
    uint8_t nri : 2;
    uint8_t f : 1;
};



#endif //RTSP_SERVER_RTP_PACKET_H