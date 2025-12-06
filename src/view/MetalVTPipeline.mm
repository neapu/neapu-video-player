//
// Created by liu86 on 2025/12/6.
//

#include "MetalVTPipeline.h"
#ifdef __APPLE__
#include <logger.h>
#import <Metal/Metal.h>
#import <CoreVideo/CoreVideo.h>
#include <rhi/qrhi.h>

namespace view {

MetalVTPipeline::MetalVTPipeline(QRhi* rhi, media::Frame::PixelFormat swFormat)
    : Pipeline(rhi)
    , m_swFormat(swFormat)
{
    NEAPU_FUNC_TRACE;
    if (m_swFormat != media::Frame::PixelFormat::NV12 &&
        m_swFormat != media::Frame::PixelFormat::P010) {
        NEAPU_LOGE("Unsupported software format for Metal VT pipeline: {}", static_cast<int>(m_swFormat));
        throw std::runtime_error("Unsupported software format for Metal VT pipeline");
    }
    m_pixelFormat = media::Frame::PixelFormat::CVPixelBuffer;

    // Get Metal device from QRhi
    const auto* nativeHandles = static_cast<const QRhiMetalNativeHandles*>(m_rhi->nativeHandles());
    if (!nativeHandles || !nativeHandles->dev) {
        NEAPU_LOGE("Failed to get Metal device from QRhi");
        throw std::runtime_error("Failed to get Metal device from QRhi");
    }
    m_metalDevice = nativeHandles->dev;

    // Create CVMetalTextureCache
    id<MTLDevice> device = (__bridge id<MTLDevice>)m_metalDevice;
    CVMetalTextureCacheRef textureCache = nullptr;
    CVReturn result = CVMetalTextureCacheCreate(
        kCFAllocatorDefault,
        nullptr,
        device,
        nullptr,
        &textureCache
    );
    
    if (result != kCVReturnSuccess || !textureCache) {
        NEAPU_LOGE("Failed to create CVMetalTextureCache, result={}", result);
        throw std::runtime_error("Failed to create CVMetalTextureCache");
    }
    
    m_metalTextureCache = textureCache;
    NEAPU_LOGD("MetalVTPipeline initialized successfully");
}

MetalVTPipeline::~MetalVTPipeline()
{
    NEAPU_FUNC_TRACE;
    if (m_yMetalTextureRef) {
        CFRelease((CVMetalTextureRef)m_yMetalTextureRef);
        m_yMetalTextureRef = nullptr;
    }
    if (m_uvMetalTextureRef) {
        CFRelease((CVMetalTextureRef)m_uvMetalTextureRef);
        m_uvMetalTextureRef = nullptr;
    }
    if (m_metalTextureCache) {
        CVMetalTextureCacheRef textureCache = (CVMetalTextureCacheRef)m_metalTextureCache;
        CFRelease(textureCache);
        m_metalTextureCache = nullptr;
    }
}

void MetalVTPipeline::updateTexture(QRhiResourceUpdateBatch* rub, media::FramePtr&& frame)
{
    CVPixelBufferRef pixelBuffer = (CVPixelBufferRef)frame->cvPixelBuffer();
    if (!pixelBuffer) {
        NEAPU_LOGE("Frame does not contain a valid CVPixelBuffer");
        return;
    }

    CVMetalTextureCacheRef textureCache = (CVMetalTextureCacheRef)m_metalTextureCache;
    
    // Release old CVMetalTextureRef objects
    if (m_yMetalTextureRef) {
        CFRelease((CVMetalTextureRef)m_yMetalTextureRef);
        m_yMetalTextureRef = nullptr;
    }
    if (m_uvMetalTextureRef) {
        CFRelease((CVMetalTextureRef)m_uvMetalTextureRef);
        m_uvMetalTextureRef = nullptr;
    }

    size_t width = CVPixelBufferGetWidth(pixelBuffer);
    size_t height = CVPixelBufferGetHeight(pixelBuffer);

    // Create Y plane texture
    CVMetalTextureRef yTextureRef = nullptr;
    MTLPixelFormat yPixelFormat = (m_swFormat == media::Frame::PixelFormat::P010) ? 
        MTLPixelFormatR16Unorm : MTLPixelFormatR8Unorm;
    
    CVReturn result = CVMetalTextureCacheCreateTextureFromImage(
        kCFAllocatorDefault,
        textureCache,
        pixelBuffer,
        nullptr,
        yPixelFormat,
        width,
        height,
        0,  // Y plane
        &yTextureRef
    );
    
    if (result != kCVReturnSuccess || !yTextureRef) {
        NEAPU_LOGE("Failed to create Y plane Metal texture, result={}", result);
        return;
    }

    // Create UV plane texture
    CVMetalTextureRef uvTextureRef = nullptr;
    MTLPixelFormat uvPixelFormat = (m_swFormat == media::Frame::PixelFormat::P010) ? 
        MTLPixelFormatRG16Unorm : MTLPixelFormatRG8Unorm;
    
    result = CVMetalTextureCacheCreateTextureFromImage(
        kCFAllocatorDefault,
        textureCache,
        pixelBuffer,
        nullptr,
        uvPixelFormat,
        width / 2,
        height / 2,
        1,  // UV plane
        &uvTextureRef
    );
    
    if (result != kCVReturnSuccess || !uvTextureRef) {
        NEAPU_LOGE("Failed to create UV plane Metal texture, result={}", result);
        CFRelease(yTextureRef);
        return;
    }

    // Get MTLTextures from CVMetalTextureRef
    id<MTLTexture> yTexture = CVMetalTextureGetTexture(yTextureRef);
    id<MTLTexture> uvTexture = CVMetalTextureGetTexture(uvTextureRef);

    if (!yTexture || !uvTexture) {
        NEAPU_LOGE("Failed to get MTLTexture from CVMetalTextureRef");
        CFRelease(yTextureRef);
        CFRelease(uvTextureRef);
        return;
    }

    // Store the CVMetalTextureRef to keep them alive
    m_yMetalTextureRef = yTextureRef;
    m_uvMetalTextureRef = uvTextureRef;

    // Update QRhiTexture with native Metal textures
    QRhiTexture::NativeTexture nativeYTex{};
    nativeYTex.object = reinterpret_cast<quint64>(yTexture);
    nativeYTex.layout = 0;

    QRhiTexture::NativeTexture nativeUVTex{};
    nativeUVTex.object = reinterpret_cast<quint64>(uvTexture);
    nativeUVTex.layout = 0;

    if (!m_yTexture->createFrom(nativeYTex) || !m_uvTexture->createFrom(nativeUVTex)) {
        NEAPU_LOGE("Failed to create QRhiTexture from native Metal texture");
        return;
    }
}

bool MetalVTPipeline::createSrb(const QSize& size)
{
    NEAPU_FUNC_TRACE;

    // Reset existing textures if they exist
    m_yTexture.reset();
    m_uvTexture.reset();

    // Create QRhiTexture wrappers
    if (m_swFormat == media::Frame::PixelFormat::NV12) {
        m_yTexture.reset(m_rhi->newTexture(
            QRhiTexture::R8,
            QSize(size.width(), size.height()), 1,
            QRhiTexture::Flags()));
        m_uvTexture.reset(m_rhi->newTexture(
            QRhiTexture::RG8,
            QSize(size.width() / 2, size.height() / 2), 1,
            QRhiTexture::Flags()));
    } else if (m_swFormat == media::Frame::PixelFormat::P010) {
        m_yTexture.reset(m_rhi->newTexture(
            QRhiTexture::R16,
            QSize(size.width(), size.height()), 1,
            QRhiTexture::Flags()));
        m_uvTexture.reset(m_rhi->newTexture(
            QRhiTexture::RG16,
            QSize(size.width() / 2, size.height() / 2), 1,
            QRhiTexture::Flags()));
    } else {
        NEAPU_LOGE("Unsupported texture format for SRB creation: {}", static_cast<int>(m_swFormat));
        return false;
    }

    // Create shader resource bindings
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

QString MetalVTPipeline::getFragmentShaderName()
{
    if (m_swFormat == media::Frame::PixelFormat::NV12) {
        return QStringLiteral(":/shaders/nv12.frag.qsb");
    }
    if (m_swFormat == media::Frame::PixelFormat::P010) {
        return QStringLiteral(":/shaders/p010.frag.qsb");
    }
    return Pipeline::getFragmentShaderName();
}

} // namespace view
#endif
