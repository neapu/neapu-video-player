#pragma once
#include <string>
#include <functional>
#include <memory>

#ifdef _WIN32
typedef struct ID3D11Device ID3D11Device;
typedef struct ID3D11DeviceContext ID3D11DeviceContext;
#endif

namespace media {
class VideoFrame;
using DecodeOverCallback =  std::function<void()>;
using PtsChangedCallback = std::function<void(int64_t currentPts)>;
using VideoFrameCallback = std::function<void(std::unique_ptr<VideoFrame>)>;
struct OpenMediaParams {
    std::string url;
    DecodeOverCallback decodeOverCallback{nullptr};
    PtsChangedCallback ptsChangedCallback{nullptr};
    // 音频和视频的出帧方式不同，音频由render拉取，视频由解码线程推送
    // 这是因为音频数据的播放需要持续稳定，而视频帧可以丢弃以保持同步
    VideoFrameCallback videoFrameCallback{nullptr};
    bool enableHWAccel{false};
    bool copyBackRender{true};
#ifdef _WIN32
    ID3D11Device* d3d11Device{nullptr};
    ID3D11DeviceContext* d3d11DeviceContext{nullptr};
#endif
};
}