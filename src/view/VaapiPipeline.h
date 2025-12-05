//
// Created by neapu on 2025/12/5.
//

#pragma once
#include "Pipeline.h"
#ifdef __linux__

class QOpenGLFunctions;

namespace view {

class VaapiPipeline : public Pipeline {
public:
    VaapiPipeline(QRhi* rhi, void* vaDisplay, void* eglDisplay, media::Frame::PixelFormat swFormat);
    ~VaapiPipeline() override;

    void updateTexture(QRhiResourceUpdateBatch* rub, media::FramePtr&& frame) override;
protected:
    bool createSrb(const QSize& size) override;
    QString getFragmentShaderName() override;

    int yGLTexture() const;
    int uvGLTexture() const;

protected:
    void* m_vaDisplay{ nullptr };
    void* m_eglDisplay{ nullptr };
    QOpenGLFunctions* m_glFuncs{ nullptr };

    std::unique_ptr<QRhiTexture> m_yTexture{};
    std::unique_ptr<QRhiTexture> m_uvTexture{};
    media::Frame::PixelFormat m_swFormat{ media::Frame::PixelFormat::None };

    void* m_eglImageY{ nullptr };
    void* m_eglImageUV{ nullptr };
};

} // namespace view
#endif