// //
// // Created by Founder on 2025/9/27.
// //
//
// #include <memory>
//
// #include "rtsp_session.h"
// #include "win10_h264_stream_media_source.h"
//
// int main() {
//     SOCKET ListenSocket = INVALID_SOCKET;
//     const auto src = new rtsp_source::win10_h264_stream_media_source();
//     rtsp::Rtsp_session session (ListenSocket, std::shared_ptr<Media_source>(src));
//     src->start_streaming(session);
//
//     char c;
//     std::cin >> c;
// }
