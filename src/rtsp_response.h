//
// Created by admin on 2025/9/23.
//

#ifndef RTSP_SERVER_RTSP_RESPONSE_H
#define RTSP_SERVER_RTSP_RESPONSE_H

#include <string>
#include <map>
#include <sstream>

namespace rtsp {
    class Rtsp_response {
    public:
        Rtsp_response(int status_code, const std::string &reason_phrase);

        void add_header(const std::string &key, const std::string &value);

        void set_body(const std::string &body);

        std::string to_string() const;

    private:
        int _statusCode;
        std::string _reasonPhrase;
        std::map<std::string, std::string> _headers;
        std::string _body;
    };
} // rtsp

#endif //RTSP_SERVER_RTSP_RESPONSE_H
