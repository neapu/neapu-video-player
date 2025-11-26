//
// Created by liu86 on 2025/11/20.
//

#pragma once
#include "DecoderBase.h"
#ifdef _WIN32
#include <d3d11.h>
#endif

typedef struct AVBufferRef AVBufferRef;
typedef struct SwsContext SwsContext;

namespace media {

class VideoDecoder : public DecoderBase {
public:
    enum class HWAccelMethod {
        None, // 软件解码
        D3D11VA, // Windows 首选
        DXVA, // Windows 备用
        Vaapi, // Linux
        VideoToolBox, // macOS
    };
#ifdef _WIN32
    VideoDecoder(AVStream* stream, const AVPacketCallback& packetCallback, HWAccelMethod hwaccelMethod, ID3D11Device* d3d11Device);
#else
    VideoDecoder(AVStream* stream, const AVPacketCallback& packetCallback, HWAccelMethod hwaccelMethod);
#endif
    ~VideoDecoder() override;

protected:
    virtual void initializeHWContext();
    virtual FramePtr convertFixelFormat(FramePtr&& avFrame);
    virtual FramePtr hwFrameTransfer(FramePtr&& avFrame);
    FramePtr postProcess(FramePtr&& frame) override;

protected:
    HWAccelMethod m_hwaccelMethod{HWAccelMethod::None};
    AVBufferRef* m_hwDeviceCtx{nullptr};
#ifdef _WIN32
    ID3D11Device* m_d3d11Device{nullptr};
#endif
    int m_hwPixelFormat{-1};
    SwsContext* m_swsCtx{nullptr};
};

} // namespace media
