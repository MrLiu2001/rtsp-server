//
// Created by admin on 2025/9/23.
//

#include "rtsp_response.h"

namespace rtsp {
    Rtsp_response::Rtsp_response(int status_code, const std::string& reason_phrase)
    : _statusCode(status_code), _reasonPhrase(reason_phrase) {}

    void Rtsp_response::add_header(const std::string& key, const std::string& value) {
        _headers[key] = value;
    }

    void Rtsp_response::set_body(const std::string& body) {
        _body = body;
        if (!_body.empty()) {
            add_header("Content-Length", std::to_string(_body.length()));
        }
    }

    std::string Rtsp_response::to_string() const {
        std::ostringstream ss;
        ss << "RTSP/1.0 " << _statusCode << " " << _reasonPhrase << "\r\n";
        for (const auto& header : _headers) {
            ss << header.first << ": " << header.second << "\r\n";
        }
        ss << "\r\n";
        if (!_body.empty()) {
            ss << _body;
        }
        return ss.str();
    }
} // rtsp