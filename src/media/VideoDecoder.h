//
// Created by liu86 on 2025/10/30.
//

#pragma once
#include "DecoderBase.h"
#ifdef _WIN32
#include <d3d11.h>
#endif

typedef struct AVBufferRef AVBufferRef;

namespace media {
enum class HWAccelType {
    None,
    D3D12VA,
    D3D11VA,
    DXVA2,
    CUDA,
    VAAPI,
    VIDEOTOOLBOX
};
struct HWAccelConfig {
    HWAccelType type{HWAccelType::None};
#ifdef _WIN32
    ID3D11Device* device{nullptr};
    ID3D11DeviceContext* context{nullptr};
#endif
};
class VideoDecoder : public DecoderBase {
public:
    ~VideoDecoder() override = default;
    bool initialize(const AVStream* stream, const HWAccelConfig& config = {});
    void destroy() override;

protected:
    bool initializeHWAccel(HWAccelType type);

protected:
    HWAccelConfig m_hwConfig{};
    AVBufferRef* m_hwDeviceCtx{nullptr};
    int m_hwPixelFormat{};
};

} // namespace media
