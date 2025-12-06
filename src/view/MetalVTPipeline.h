//
// Created by liu86 on 2025/12/6.
//

#pragma once
#include "Pipeline.h"
#ifdef __APPLE__

namespace view {

class MetalVTPipeline : public Pipeline {
public:
    MetalVTPipeline(QRhi* rhi, media::Frame::PixelFormat swFormat);
    ~MetalVTPipeline() override;

    void updateTexture(QRhiResourceUpdateBatch* rub, media::FramePtr&& frame) override;

protected:
    bool createSrb(const QSize& size) override;
    QString getFragmentShaderName() override;

    std::unique_ptr<QRhiTexture> m_yTexture{};
    std::unique_ptr<QRhiTexture> m_uvTexture{};

    void* m_metalDevice{ nullptr };
    void* m_metalTextureCache{ nullptr };  // CVMetalTextureCacheRef
    void* m_yMetalTextureRef{ nullptr };   // CVMetalTextureRef
    void* m_uvMetalTextureRef{ nullptr };  // CVMetalTextureRef
    media::Frame::PixelFormat m_swFormat{ media::Frame::PixelFormat::None };
};

} // namespace view
#endif
