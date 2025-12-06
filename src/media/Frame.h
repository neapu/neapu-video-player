//
// Created by liu86 on 2025/11/18.
//

#pragma once
#include <memory>
#include <cstdint>

typedef struct AVFrame AVFrame;

#ifdef _WIN32
struct ID3D11Texture2D;
#endif

namespace media {
class Frame final {
public:
    enum class FrameType {
        Normal,
        Flush,
        EndOfStream,
    };
    explicit Frame(FrameType type, int serial);
    ~Frame();

    Frame(const Frame& other) = delete;
    Frame& operator=(const Frame& other) = delete;
    Frame(Frame&& other) noexcept;
    Frame& operator=(Frame&& other) noexcept;

    int serial() const { return m_serial; }
    FrameType type() const { return m_type; }

    void copyMetaDataFrom(const Frame& other);

    const uint8_t* data(int index) const;
    int lineSize(int index) const;

    // video
    int width() const;
    int height() const;
    int64_t ptsUs() const; // us
    int64_t durationUs() const; // us
    enum class PixelFormat {
        None,
        YUV420P,
        NV12,
        P010,
        D3D11Texture2D,
        Vaapi,
        CVPixelBuffer,  // macOS VideoToolbox output format
    };
    PixelFormat pixelFormat() const;
    enum class ColorSpace {
        BT601, // 其他全部退化到BT601
        BT709,
    };
    ColorSpace colorSpace() const;
    enum class ColorRange {
        Limited,
        Full,
    };
    ColorRange colorRange() const;
    uint8_t* yData() const;
    uint8_t* uData() const;
    uint8_t* vData() const;
    int yLineSize() const;
    int uLineSize() const;
    int vLineSize() const;
#ifdef _WIN32
    ID3D11Texture2D* d3d11Texture2D() const;
    int subresourceIndex() const;
#endif

#ifdef __linux__
    unsigned int vaapiSurfaceId() const;
#endif

#ifdef __APPLE__
    void* cvPixelBuffer() const;
#endif

    // audio, 格式固定为S16LE
    int sampleRate() const;
    int channels() const;
    int64_t nbSamples() const;

    AVFrame* avFrame();

    PixelFormat swFormat();

private:
    FrameType m_type{FrameType::Normal};
    AVFrame* m_avFrame{nullptr};
    int m_serial{0};
};
using FramePtr = std::unique_ptr<Frame>;
} // namespace media
