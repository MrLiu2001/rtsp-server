//
// Created by admin on 2025/9/23.
//

#ifndef RTSP_SERVER_RTSP_REQUEST_H
#define RTSP_SERVER_RTSP_REQUEST_H

#include <string>
#include <map>
#include <sstream>

namespace rtsp {
    class Rtsp_request {
    public:
        enum Method { OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN, UNKNOWN };
        static Rtsp_request parse(const std::string& raw_request);

        Method method() const { return _method; }
        const std::string& url() const { return _url; }
        int cseq() const;
        std::string get_header(const std::string& header_name) const;


    private:
        Method _method = UNKNOWN;
        std::string _url;
        std::string _version;
        int _cseq = 0;
        std::map<std::string, std::string> _headers;
    };
} // rtsp

#endif //RTSP_SERVER_RTSP_REQUEST_H