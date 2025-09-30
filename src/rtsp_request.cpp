//
// Created by admin on 2025/9/23.
//

#include "rtsp_request.h"

namespace rtsp {
    Rtsp_request Rtsp_request::parse(const std::string& raw_request) {
        Rtsp_request req;
        std::istringstream ss(raw_request);
        std::string line;

        // Parse request line
        std::getline(ss, line);
        std::istringstream line_ss(line);
        std::string method_str;
        line_ss >> method_str >> req._url >> req._version;

        if (method_str == "OPTIONS") req._method = OPTIONS;
        else if (method_str == "DESCRIBE") req._method = DESCRIBE;
        else if (method_str == "SETUP") req._method = SETUP;
        else if (method_str == "PLAY") req._method = PLAY;
        else if (method_str == "TEARDOWN") req._method = TEARDOWN;
        else req._method = UNKNOWN;

        // Parse headers
        while (std::getline(ss, line) && line != "\r" && !line.empty()) {
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);
                // Trim leading whitespace from value
                value.erase(0, value.find_first_not_of(" \t"));
                // Trim trailing \r
                if (!value.empty() && value.back() == '\r') {
                    value.pop_back();
                }
                req._headers[key] = value;
            }
        }

        req._cseq = std::stoi(req.get_header("CSeq"));
        return req;
    }

    std::string Rtsp_request::get_header(const std::string& header_name) const {
        const auto it = _headers.find(header_name);
        if (it != _headers.end()) {
            return it->second;
        }
        return "";
    }

    int Rtsp_request::cseq() const {
        return _cseq;
    }

} // rtsp