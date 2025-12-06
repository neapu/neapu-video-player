//
// Created by neapu on 2025/12/6.
//

#pragma once
#include "Pipeline.h"
#ifdef __APPLE__

namespace view {

class MetalPipeline : public Pipeline {
public:
    MetalPipeline(QRhi* rhi, media::Frame::PixelFormat swFormat);
    ~MetalPipeline() override;

    void updateTexture(QRhiResourceUpdateBatch* rub, media::FramePtr&& frame) override;
protected:
    bool createSrb(const QSize& size) override;
    QString getFragmentShaderName() override;

    void* metalYTexture() const;
    void* metalUVTexture() const;

protected:
    std::unique_ptr<QRhiTexture> m_yTexture{};
    std::unique_ptr<QRhiTexture> m_uvTexture{};
    media::Frame::PixelFormat m_swFormat{ media::Frame::PixelFormat::None };
    
    void* m_metalTextureY{ nullptr };
    void* m_metalTextureUV{ nullptr };
};

} // namespace view
#endif
