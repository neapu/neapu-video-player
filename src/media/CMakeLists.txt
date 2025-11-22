set(LIB_NAME "media")

if (FFMPEG_DIR)
    message(STATUS "FFMPEG_DIR is set to: ${FFMPEG_DIR}")
else ()
    message(FATAL_ERROR "FFMPEG_DIR is not set. Please set FFMPEG_DIR to the path of your FFMPEG installation.")
endif()

set(FFMPEG_INCLUDE_DIR ${FFMPEG_DIR}/include)
set(FFMPEG_LIB_DIR ${FFMPEG_DIR}/lib)
find_library(AVFORMAT_LIBRARY NAMES avformat PATHS ${FFMPEG_LIB_DIR})
message(STATUS "AVFORMAT_LIBRARY: ${AVFORMAT_LIBRARY}")
find_library(AVCODEC_LIBRARY NAMES avcodec PATHS ${FFMPEG_LIB_DIR})
message(STATUS "AVCODEC_LIBRARY: ${AVCODEC_LIBRARY}")
find_library(AVUTIL_LIBRARY NAMES avutil PATHS ${FFMPEG_LIB_DIR})
message(STATUS "AVUTIL_LIBRARY: ${AVUTIL_LIBRARY}")
find_library(SWSCALE_LIBRARY NAMES swscale PATHS ${FFMPEG_LIB_DIR})
message(STATUS "SWSCALE_LIBRARY: ${SWSCALE_LIBRARY}")
find_library(SWRESAMPLE_LIBRARY NAMES swresample PATHS ${FFMPEG_LIB_DIR})
message(STATUS "SWRESAMPLE_LIBRARY: ${SWRESAMPLE_LIBRARY}")

add_library(${LIB_NAME} STATIC
        MediaDecoder.cpp
        MediaDecoder.h
        Frame.cpp
        Frame.h
        MediaDecoderImpl.cpp
        MediaDecoderImpl.h
        Demuxer.cpp
        Demuxer.h
        Helper.cpp
        Helper.h
        DecoderBase.cpp
        DecoderBase.h
        VideoDecoder.cpp
        VideoDecoder.h
        AudioDecoder.cpp
        AudioDecoder.h
)

target_link_libraries(${LIB_NAME} PUBLIC logger)
target_include_directories(${LIB_NAME} PUBLIC ${FFMPEG_INCLUDE_DIR})
target_link_libraries(${LIB_NAME} PUBLIC
        ${AVFORMAT_LIBRARY}
        ${AVCODEC_LIBRARY}
        ${AVUTIL_LIBRARY}
        ${SWSCALE_LIBRARY}
        ${SWRESAMPLE_LIBRARY}
)