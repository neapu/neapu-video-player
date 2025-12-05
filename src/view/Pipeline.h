//
// Created by liu86 on 2025/12/4.
//

#pragma once
#include <rhi/qrhi.h>
#include <memory>
#include "../media/Frame.h"
#ifdef _WIN32
#include <d3d11.h>
#endif

namespace view {

class Pipeline {
public:
    explicit Pipeline(QRhi* rhi);
    virtual ~Pipeline();

    struct CreateParam {
        QRhi* rhi{nullptr};
#ifdef _WIN32
        ID3D11Device* d3d11Device{nullptr};
        ID3D11DeviceContext* d3d11DeviceContext{nullptr};
#elifdef  __linux__
        void* eglDisplay{ nullptr };
#endif
    };
    static std::unique_ptr<Pipeline> makeFormFrame(const media::FramePtr& frame, const CreateParam& param);

    bool initialize(QRhiRenderTarget* renderTarget, const QSize& size = QSize{0, 0});

    virtual void updateTexture(QRhiResourceUpdateBatch* rub, media::FramePtr&& frame) {}

    QRhiGraphicsPipeline* getPipeline() const;
    QRhiShaderResourceBindings* getSrb() const;

    media::Frame::PixelFormat pixelFormat() const { return m_pixelFormat; }
    bool checkFormat(const media::FramePtr& frame) const;
    void updateVertexUniforms(QRhiResourceUpdateBatch* rub, const QSize& renderSize, const QSize& frameSize);
    void updateColorParamsIfNeeded(const media::FramePtr& frame, QRhiResourceUpdateBatch* rub);

protected:
    virtual bool createSrb(const QSize& size);
    virtual QString getFragmentShaderName();
    virtual bool createPipeline(QRhiRenderTarget* renderTarget);

protected:
    media::Frame::PixelFormat m_pixelFormat{media::Frame::PixelFormat::None};
    media::Frame::ColorSpace m_colorSpace{media::Frame::ColorSpace::BT601};
    media::Frame::ColorRange m_colorRange{media::Frame::ColorRange::Limited};
    bool m_colorParamsInitialized{false};
    QRhi* m_rhi{nullptr};

    QSize m_oldFrameSize{0,0};
    QSize m_oldRenderSize{0,0};

    std::unique_ptr<QRhiGraphicsPipeline> m_pipeline{};
    std::unique_ptr<QRhiSampler> m_sampler{};
    std::unique_ptr<QRhiShaderResourceBindings> m_srb{};
    std::unique_ptr<QRhiBuffer> m_vsUBuffer{};
    std::unique_ptr<QRhiBuffer> m_colorParamsUBuffer{}; // 用于颜色转换参数的uniform缓冲区 (mat3 + float, std140布局)
};

} // namespace view
