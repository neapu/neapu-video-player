//
// Created by liu86 on 2025/10/30.
//

#pragma once
#include <memory>
#include "pub.h"
extern "C"{
#include <libavutil/avutil.h>
}
#ifdef _WIN32
#include <d3d11.h>
#include <wrl/client.h>
#endif

namespace media {
class VideoFrame {
public:
    enum class PixelFormat {
        NONE,
        YUV420P,
        D3D11
    };

    enum class ColorRange {
        LIMITED,
        FULL
    };

    enum class ColorSpace {
        BT601,
        BT709,
        BT2020
    };

    enum class ColorTransfer {
        BT709,
        HDR10,
        HLOG
    };

    ~VideoFrame();

    static std::unique_ptr<VideoFrame> fromAVFrame(const void* frame, AVRational timeBase);

    uint8_t* y() const { return m_yData; }
    uint8_t* u() const { return m_uData; }
    uint8_t* v() const { return m_vData; }
    size_t ySize() const { return m_ySize; }
    size_t uSize() const { return m_uSize; }
    size_t vSize() const { return m_vSize; }
    int width() const { return m_width; }
    int height() const { return m_height; }

    int64_t duration() const { return m_durationUs; }
    int64_t pts() const { return m_pts; }
    void setPts(int64_t pts) { m_pts = pts; }

    PixelFormat pixelFormat() const { return m_pixelFormat; }

    ColorSpace colorSpace() const { return m_colorSpace; }
    ColorRange colorRange() const { return m_colorRange; }
    ColorTransfer colorTransfer() const { return m_colorTransfer; }

#ifdef _WIN32
    ID3D11Texture2D* getD3D11Texture() const { return d3d11Texture.Get(); }
#endif

private:
    uint8_t* m_yData{nullptr};
    size_t m_ySize{0};
    uint8_t* m_uData{nullptr};
    size_t m_uSize{0};
    uint8_t* m_vData{nullptr};
    size_t m_vSize{0};
    int m_width{0};
    int m_height{0};

    PixelFormat m_pixelFormat{PixelFormat::YUV420P};

    int64_t m_durationUs{0};
    int64_t m_pts{0};

    ColorRange m_colorRange{ColorRange::LIMITED};
    ColorSpace m_colorSpace{ColorSpace::BT709};
    ColorTransfer m_colorTransfer{ColorTransfer::BT709};

#ifdef _WIN32
    Microsoft::WRL::ComPtr<ID3D11Texture2D> d3d11Texture{nullptr};
#endif
};
using VideoFramePtr = std::unique_ptr<VideoFrame>;
} // namespace media
