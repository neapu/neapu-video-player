//
// Created by liu86 on 2025/12/4.
//

#include "D3D11VAPipeline.h"

#include "logger.h"
#ifdef _WIN32
namespace view {
D3D11VAPipeline::D3D11VAPipeline(QRhi* rhi, ID3D11Device* d3d11Device, ID3D11DeviceContext* d3d11DeviceContext, media::Frame::PixelFormat swFormat)
    : Pipeline(rhi),
      m_d3d11Device(d3d11Device),
      m_d3d11DeviceContext(d3d11DeviceContext)
{
    if (swFormat == media::Frame::PixelFormat::NV12) {
        m_textureFormat = DXGI_FORMAT_NV12;
    } else if (swFormat == media::Frame::PixelFormat::P010) {
        m_textureFormat = DXGI_FORMAT_P010;
    } else {
        NEAPU_LOGE("Unsupported software format for D3D11VA pipeline: {}", static_cast<int>(swFormat));
        throw std::runtime_error("Unsupported software format for D3D11VA pipeline");
    }
    m_pixelFormat = media::Frame::PixelFormat::D3D11Texture2D;
}
D3D11VAPipeline::~D3D11VAPipeline() {}

void D3D11VAPipeline::updateTexture(QRhiResourceUpdateBatch* rub, media::FramePtr&& frame)
{
    D3D11_BOX srcBox{};
    srcBox.left = 0;
    srcBox.top = 0;
    srcBox.front = 0;
    srcBox.right = static_cast<UINT>(frame->width());
    srcBox.bottom = static_cast<UINT>(frame->height());
    srcBox.back = 1;

    auto* texture = frame->d3d11Texture2D();
    if (!texture) {
        NEAPU_LOGE("Frame does not contain a valid D3D11 texture");
        return;
    }

    m_d3d11DeviceContext->CopySubresourceRegion(
        m_nativeTexture.Get(),
        0, 0, 0, 0,
        texture,
        frame->subresourceIndex(),
        &srcBox);
}
bool D3D11VAPipeline::createSrb(const QSize& size)
{
    NEAPU_FUNC_TRACE;

    m_nativeTexture.Reset();
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = size.width();
    desc.Height = size.height();
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = m_textureFormat;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    auto hr = m_d3d11Device->CreateTexture2D(&desc, nullptr, m_nativeTexture.GetAddressOf());
    if (FAILED(hr)) {
        NEAPU_LOGE("Failed to create native D3D11 texture, hr={}", static_cast<unsigned int>(hr));
        return false;
    }

    // 创建QRhiTexture
    QRhiTexture::NativeTexture nativeTex{};
    nativeTex.object = reinterpret_cast<quint64>(m_nativeTexture.Get());
    nativeTex.layout = 0; // d3d11不需要

    if (m_textureFormat == DXGI_FORMAT_NV12) {
        m_yTexture.reset(m_rhi->newTexture(
            QRhiTexture::R8,
            QSize(size.width(), size.height()), 1,
            QRhiTexture::Flags()));
        m_uvTexture.reset(m_rhi->newTexture(
            QRhiTexture::RG8,
            QSize(size.width() / 2, size.height() / 2), 1,
            QRhiTexture::Flags()));
    } else if (m_textureFormat == DXGI_FORMAT_P010) {
        m_yTexture.reset(m_rhi->newTexture(
            QRhiTexture::R16,
            QSize(size.width(), size.height()), 1,
            QRhiTexture::Flags()));
        m_uvTexture.reset(m_rhi->newTexture(
            QRhiTexture::RG16,
            QSize(size.width() / 2, size.height() / 2), 1,
            QRhiTexture::Flags()));
    } else {
        NEAPU_LOGE("Unsupported texture format for SRB creation: {}", static_cast<int>(m_textureFormat));
        return false;
    }

    if (m_yTexture->createFrom(nativeTex) == false ||
        m_uvTexture->createFrom(nativeTex) == false) {
        NEAPU_LOGE("Failed to create QRhiTextures from native D3D11 texture");
        m_yTexture.reset();
        m_uvTexture.reset();
        return false;
    }

    m_srb.reset(m_rhi->newShaderResourceBindings());
    m_srb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, m_yTexture.get(), m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_uvTexture.get(), m_sampler.get()),
        QRhiShaderResourceBinding::uniformBuffer(3, QRhiShaderResourceBinding::VertexStage, m_vsUBuffer.get()),
        QRhiShaderResourceBinding::uniformBuffer(4, QRhiShaderResourceBinding::FragmentStage, m_colorParamsUBuffer.get()),
    });
    if (!m_srb->create()) {
        NEAPU_LOGE("Failed to create shader resource bindings");
        m_srb.reset();
        return false;
    }

    return true;
}
QString D3D11VAPipeline::getFragmentShaderName()
{
    if (m_textureFormat == DXGI_FORMAT_NV12) {
        return QStringLiteral(":/shaders/nv12.frag.qsb");
    } else if (m_textureFormat == DXGI_FORMAT_P010) {
        return QStringLiteral(":/shaders/p010.frag.qsb");
    }
    return Pipeline::getFragmentShaderName();
}
} // namespace view
#endif