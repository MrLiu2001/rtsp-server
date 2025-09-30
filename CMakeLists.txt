cmake_minimum_required(VERSION 4.0)
project(rtsp_server)

set(CMAKE_CXX_STANDARD 11)

include_directories(./include)
link_directories(./lib)

add_executable(rtsp_server src/main.cpp
        src/rtsp_server.cpp
        src/rtsp_server.h
        src/rtsp_session.cpp
        src/rtsp_session.h
        src/rtsp_request.cpp
        src/rtsp_request.h
        src/rtsp_response.cpp
        src/rtsp_response.h
        src/media_source.h
        src/h264_file_media_source.cpp
        src/h264_file_media_source.h
        src/socket_util.h
        src/rtp_packet.h
        src/win10_h264_stream_media_source.cpp
        src/win10_h264_stream_media_source.h
        src/test.cpp
        src/ffmpeg_helper.h
        src/rtp_helper.h
        src/media_source.cpp)

if (WIN32)
    target_link_libraries(rtsp_server ws2_32)
    target_link_libraries(rtsp_server gdiplus)
    target_link_libraries(rtsp_server
            avcodec
            avformat
            swscale
            avutil)
else ()
    target_link_libraries(rtsp_server pthread)
endif ()