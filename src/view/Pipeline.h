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
    Pipeline(media::Frame::PixelFormat pixFmt, QRhi* rhi, QRhiRenderTarget* renderTarget);
    virtual ~Pipeline();

    static std::unique_ptr<Pipeline> createForFrame(const media::FramePtr& frame, QRhi* rhi, QRhiRenderTarget* renderTarget);
#ifdef _WIN32
    static std::unique_ptr<Pipeline> createForFrame(
        const media::FramePtr& frame, QRhi* rhi,
        QRhiRenderTarget* renderTarget, ID3D11Device* d3d11Device, ID3D11DeviceContext* d3d11DeviceContext);
#endif

    virtual bool create(const media::FramePtr& frame, QRhiCommandBuffer* cb);

    virtual QRhiGraphicsPipeline* getPipeline();
    virtual QRhiShaderResourceBindings* getSrb();

    virtual media::Frame::PixelFormat pixelFormat() const { return m_pixelFormat; }
    virtual int width() const { return m_width; }
    virtual int height() const { return m_height; }

    virtual bool checkFormat(const media::FramePtr& frame) const;
    virtual void updateTexture(QRhiResourceUpdateBatch* rub, media::FramePtr&& frame) {}
    virtual void updateVertexUniforms(QRhiResourceUpdateBatch* rub, const QSize& renderSize);
    void updateColorParamsIfNeeded(const media::FramePtr& frame, QRhiResourceUpdateBatch* rub);

protected:
    virtual bool createSrb();
    virtual QString getFragmentShaderName();
    virtual bool createPipeline();

protected:
    media::Frame::PixelFormat m_pixelFormat{media::Frame::PixelFormat::None};
    media::Frame::ColorSpace m_colorSpace{media::Frame::ColorSpace::BT601};
    media::Frame::ColorRange m_colorRange{media::Frame::ColorRange::Limited};
    bool m_colorParamsInitialized{false};
    QRhi* m_rhi{nullptr};
    QRhiRenderTarget * m_renderTarget{nullptr};

    int m_width{0};
    int m_height{0};

    QSize m_oldFrameSize{0,0};
    QSize m_oldRenderSize{0,0};

    std::unique_ptr<QRhiGraphicsPipeline> m_pipeline{};
    std::unique_ptr<QRhiSampler> m_sampler{};
    std::unique_ptr<QRhiShaderResourceBindings> m_srb{};
    std::unique_ptr<QRhiBuffer> m_vsUBuffer{};
    std::unique_ptr<QRhiBuffer> m_colorParamsUBuffer{}; // 用于颜色转换参数的uniform缓冲区 (mat3 + float, std140布局)

    // 保持帧引用直到下一帧到来，确保纹理上传完成前数据有效
    media::FramePtr m_currentFrame{};
};

} // namespace view
