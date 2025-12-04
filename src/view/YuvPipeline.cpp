//
// Created by liu86 on 2025/12/4.
//

#include "YuvPipeline.h"
#include <logger.h>

namespace view {
YuvPipeline::YuvPipeline(QRhi* rhi, QRhiRenderTarget* renderTarget)
    : Pipeline(media::Frame::PixelFormat::YUV420P, rhi, renderTarget)
{
}
YuvPipeline::~YuvPipeline() = default;

void YuvPipeline::updateTexture(QRhiResourceUpdateBatch* rub, media::FramePtr&& frame)
{
    if (!frame) {
        NEAPU_LOGE("Frame is null");
        return;
    }

    if (m_width != frame->width() || m_height != frame->height()) {
        m_width = frame->width();
        m_height = frame->height();
        if (!createSrb()) {
            NEAPU_LOGE("Failed to recreate SRB for YUV pipeline");
            return;
        }
    }

    // Y平面
    {
        int yDataSize = frame->lineSize(0) * frame->height();
        QRhiTextureSubresourceUploadDescription sub(frame->data(0), yDataSize);
        sub.setSourceSize(QSize(m_width, m_height));
        sub.setDataStride(frame->lineSize(0));
        QRhiTextureUploadEntry entry(0, 0, sub);
        QRhiTextureUploadDescription desc({entry});
        rub->uploadTexture(m_yTexture.get(), desc);
    }

    // U平面
    {
        int uDataSize = frame->lineSize(1) * (frame->height() / 2);
        QRhiTextureSubresourceUploadDescription sub(frame->data(1), uDataSize);
        sub.setSourceSize(QSize(m_width / 2, m_height / 2));
        sub.setDataStride(frame->lineSize(1));
        QRhiTextureUploadEntry entry(0, 0, sub);
        QRhiTextureUploadDescription desc({entry});
        rub->uploadTexture(m_uTexture.get(), desc);
    }

    // V平面
    {
        int vDataSize = frame->lineSize(2) * (frame->height() / 2);
        QRhiTextureSubresourceUploadDescription sub(frame->data(2), vDataSize);
        sub.setSourceSize(QSize(m_width / 2, m_height / 2));
        sub.setDataStride(frame->lineSize(2));
        QRhiTextureUploadEntry entry(0, 0, sub);
        QRhiTextureUploadDescription desc({entry});
        rub->uploadTexture(m_vTexture.get(), desc);
    }
    
    // frame 在这里析构
}
bool YuvPipeline::createSrb()
{
    NEAPU_FUNC_TRACE;
    
    // 先释放旧的纹理
    m_yTexture.reset();
    m_uTexture.reset();
    m_vTexture.reset();
    
    // 创建新纹理
    m_yTexture.reset(m_rhi->newTexture(QRhiTexture::R8, QSize(m_width, m_height), 1, QRhiTexture::Flags()));
    m_uTexture.reset(m_rhi->newTexture(QRhiTexture::R8, QSize(m_width / 2, m_height / 2), 1, QRhiTexture::Flags()));
    m_vTexture.reset(m_rhi->newTexture(QRhiTexture::R8, QSize(m_width / 2, m_height / 2), 1, QRhiTexture::Flags()));
    if (!m_yTexture->create() || !m_uTexture->create() || !m_vTexture->create()) {
        NEAPU_LOGE("Failed to create YUV textures");
        m_yTexture.reset();
        m_uTexture.reset();
        m_vTexture.reset();
        return false;
    }

    // 总是重新创建 SRB，不复用旧的
    m_srb.reset(m_rhi->newShaderResourceBindings());
    m_srb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, m_yTexture.get(), m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_uTexture.get(), m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, m_vTexture.get(), m_sampler.get()),
        QRhiShaderResourceBinding::uniformBuffer(3, QRhiShaderResourceBinding::VertexStage, m_vsUBuffer.get()),
        QRhiShaderResourceBinding::uniformBuffer(4, QRhiShaderResourceBinding::FragmentStage, m_colorParamsUBuffer.get()),
    });
    if (!m_srb->create()) {
        NEAPU_LOGE("Failed to create shader resource bindings for YUV pipeline");
        m_srb.reset();
        return false;
    }

    return true;
}
QString YuvPipeline::getFragmentShaderName()
{
    return ":/shaders/yuv420p.frag.qsb";
}

} // namespace view