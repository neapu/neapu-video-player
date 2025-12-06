set(LIB_NAME "media")

if (FFMPEG_DIR)
    message(STATUS "FFMPEG_DIR is set to: ${FFMPEG_DIR}")
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
else ()
    message(STATUS "FFMPEG_DIR is not set. Trying to find FFmpeg using PkgConfig...")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
    pkg_check_modules(AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
    pkg_check_modules(AVUTIL REQUIRED IMPORTED_TARGET libavutil)
    pkg_check_modules(SWSCALE REQUIRED IMPORTED_TARGET libswscale)
    pkg_check_modules(SWRESAMPLE REQUIRED IMPORTED_TARGET libswresample)
    message(STATUS "Found FFmpeg via PkgConfig")
    set(FFMPEG_INCLUDE_DIR ${AVFORMAT_INCLUDE_DIRS})
endif()

add_library(${LIB_NAME} STATIC
        Frame.cpp
        Frame.h
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
        Packet.cpp
        Packet.h
        Queue.cpp
        Queue.h
        Player.cpp
        Player.h
        PlayerImpl.cpp
        PlayerImpl.h
)

target_link_libraries(${LIB_NAME} PUBLIC logger)
target_include_directories(${LIB_NAME} PUBLIC ${FFMPEG_INCLUDE_DIR})

if (FFMPEG_DIR)
    target_link_libraries(${LIB_NAME} PUBLIC
            ${AVFORMAT_LIBRARY}
            ${AVCODEC_LIBRARY}
            ${AVUTIL_LIBRARY}
            ${SWSCALE_LIBRARY}
            ${SWRESAMPLE_LIBRARY}
    )
else ()
    target_link_libraries(${LIB_NAME} PUBLIC
            PkgConfig::AVFORMAT
            PkgConfig::AVCODEC
            PkgConfig::AVUTIL
            PkgConfig::SWSCALE
            PkgConfig::SWRESAMPLE
    )
endif()