//
// Created by Founder on 2025/9/28.
//

#ifndef RTSP_SERVER_RTP_HELPER_H
#define RTSP_SERVER_RTP_HELPER_H
#include <cstdint>

enum class Rtp_Transport_Type {UDP, TCP};

// Helper to find NAL unit start codes
inline const uint8_t *find_nal_unit_start(const uint8_t *start, const uint8_t *end) {
    for (const uint8_t *p = start; p + 3 < end; ++p) {
        if (p[0] == 0x00 && p[1] == 0x00) {
            if (p[2] == 0x01) {
                return p;
            }
            if (p[2] == 0x00 && p[3] == 0x01) {
                return p;
            }
        }
    }
    return end;
}

inline const uint8_t *find_nal_unit_end(const uint8_t *start, const uint8_t *end) {
    for (const uint8_t *p = start; p + 3 < end; ++p) {
        if (p[0] == 0x00 && p[1] == 0x00) {
            if (p[2] == 0x01) {
                return p + 3;
            }
            if (p[2] == 0x00 && p[3] == 0x01) {
                return p + 4;
            }
        }
    }
    return end;
}

inline const uint8_t *find_start_code(const uint8_t *begin, const uint8_t *end) {
    for (const uint8_t *p = begin; p + 3 < end; ++p) {
        if (p[0] == 0x00 && p[1] == 0x00) {
            if (p[2] == 0x01) {
                return p + 3;
            }
            if (p[2] == 0x00 && p[3] == 0x01) {
                return p + 4;
            }
        }
    }
    return end;
}

inline const uint8_t* find_start_code_boundary(const uint8_t* begin, const uint8_t* end) {
    for (const uint8_t* p = begin; p + 3 < end; ++p) {
        if (p[0] == 0 && p[1] == 0) {
            if (p[2] == 1) return p;
            if (p[2] == 0 && (p + 3 < end) && p[3] == 1) return p;
        }
    }
    return nullptr;
}

#endif //RTSP_SERVER_RTP_HELPER_H