//
// Created by neapu on 2025/12/6.
//

#include "MetalPipeline.h"
#ifdef __APPLE__
#include <logger.h>
#include <rhi/qrhi.h>

#import <Metal/Metal.h>
#import <CoreVideo/CoreVideo.h>

namespace view {

MetalPipeline::MetalPipeline(QRhi* rhi, media::Frame::PixelFormat swFormat)
    : Pipeline(rhi)
    , m_swFormat(swFormat)
{
    NEAPU_FUNC_TRACE;
    if (m_swFormat != media::Frame::PixelFormat::NV12 &&
        m_swFormat != media::Frame::PixelFormat::P010) {
        NEAPU_LOGE("Unsupported VideoToolbox software format: {}", static_cast<int>(m_swFormat));
        throw std::runtime_error("Unsupported VideoToolbox software format");
    }
    m_pixelFormat = media::Frame::PixelFormat::VideoToolbox;
}

MetalPipeline::~MetalPipeline()
{
    NEAPU_FUNC_TRACE;
    if (m_metalTextureY) {
        CFRelease(m_metalTextureY);
        m_metalTextureY = nullptr;
    }
    if (m_metalTextureUV) {
        CFRelease(m_metalTextureUV);
        m_metalTextureUV = nullptr;
    }
}

void MetalPipeline::updateTexture(QRhiResourceUpdateBatch* rub, media::FramePtr&& frame)
{
    CVPixelBufferRef pixelBuffer = static_cast<CVPixelBufferRef>(frame->cvPixelBuffer());
    if (!pixelBuffer) {
        NEAPU_LOGE("Frame does not contain a valid CVPixelBuffer");
        return;
    }

    // 获取 Metal 设备
    const auto* nativeHandles = static_cast<const QRhiMetalNativeHandles*>(m_rhi->nativeHandles());
    if (!nativeHandles || !nativeHandles->dev) {
        NEAPU_LOGE("Failed to get Metal device from QRhi");
        return;
    }
    
    id<MTLDevice> device = (__bridge id<MTLDevice>)nativeHandles->dev;

    // 创建 CVMetalTextureCache (静态变量，整个程序生命周期复用)
    static CVMetalTextureCacheRef textureCache = nullptr;
    if (!textureCache) {
        CVReturn result = CVMetalTextureCacheCreate(
            kCFAllocatorDefault,
            nullptr,
            device,
            nullptr,
            &textureCache
        );
        if (result != kCVReturnSuccess) {
            NEAPU_LOGE("Failed to create CVMetalTextureCache: {}", result);
            return;
        }
    }

    // 清理旧的 CVMetalTexture（每帧都会创建新的）
    if (m_metalTextureY) {
        CFRelease(m_metalTextureY);
        m_metalTextureY = nullptr;
    }
    if (m_metalTextureUV) {
        CFRelease(m_metalTextureUV);
        m_metalTextureUV = nullptr;
    }

    CVMetalTextureRef yTexture = nullptr;
    CVMetalTextureRef uvTexture = nullptr;

    // 根据格式从 CVPixelBuffer 创建 Metal 纹理（零拷贝）
    if (m_swFormat == media::Frame::PixelFormat::NV12) {
        // Y 平面 (R8Unorm)
        CVReturn result = CVMetalTextureCacheCreateTextureFromImage(
            kCFAllocatorDefault,
            textureCache,
            pixelBuffer,
            nullptr,
            MTLPixelFormatR8Unorm,
            CVPixelBufferGetWidthOfPlane(pixelBuffer, 0),
            CVPixelBufferGetHeightOfPlane(pixelBuffer, 0),
            0, // Y plane index
            &yTexture
        );
        if (result != kCVReturnSuccess) {
            NEAPU_LOGE("Failed to create Y texture from CVPixelBuffer: {}", result);
            return;
        }

        // UV 平面 (RG8Unorm)
        result = CVMetalTextureCacheCreateTextureFromImage(
            kCFAllocatorDefault,
            textureCache,
            pixelBuffer,
            nullptr,
            MTLPixelFormatRG8Unorm,
            CVPixelBufferGetWidthOfPlane(pixelBuffer, 1),
            CVPixelBufferGetHeightOfPlane(pixelBuffer, 1),
            1, // UV plane index
            &uvTexture
        );
        if (result != kCVReturnSuccess) {
            NEAPU_LOGE("Failed to create UV texture from CVPixelBuffer: {}", result);
            CFRelease(yTexture);
            return;
        }
    } else if (m_swFormat == media::Frame::PixelFormat::P010) {
        // Y 平面 (R16Unorm)
        CVReturn result = CVMetalTextureCacheCreateTextureFromImage(
            kCFAllocatorDefault,
            textureCache,
            pixelBuffer,
            nullptr,
            MTLPixelFormatR16Unorm,
            CVPixelBufferGetWidthOfPlane(pixelBuffer, 0),
            CVPixelBufferGetHeightOfPlane(pixelBuffer, 0),
            0, // Y plane index
            &yTexture
        );
        if (result != kCVReturnSuccess) {
            NEAPU_LOGE("Failed to create Y texture from CVPixelBuffer: {}", result);
            return;
        }

        // UV 平面 (RG16Unorm)
        result = CVMetalTextureCacheCreateTextureFromImage(
            kCFAllocatorDefault,
            textureCache,
            pixelBuffer,
            nullptr,
            MTLPixelFormatRG16Unorm,
            CVPixelBufferGetWidthOfPlane(pixelBuffer, 1),
            CVPixelBufferGetHeightOfPlane(pixelBuffer, 1),
            1, // UV plane index
            &uvTexture
        );
        if (result != kCVReturnSuccess) {
            NEAPU_LOGE("Failed to create UV texture from CVPixelBuffer: {}", result);
            CFRelease(yTexture);
            return;
        }
    }

    m_metalTextureY = yTexture;
    m_metalTextureUV = uvTexture;

    // 从 CVMetalTexture 获取 MTLTexture 对象
    id<MTLTexture> mtlYTexture = CVMetalTextureGetTexture(yTexture);
    id<MTLTexture> mtlUVTexture = CVMetalTextureGetTexture(uvTexture);

    if (!mtlYTexture || !mtlUVTexture) {
        NEAPU_LOGE("Failed to get Metal texture from CVMetalTexture");
        return;
    }

    // 使用原生 Metal 纹理重建 QRhiTexture
    // 注意：由于 Metal 的限制，必须重新创建 QRhiTexture 来包装新的原生纹理
    // 但 SRB 可以保持不变，只需要更新纹理绑定
    QRhiTexture::NativeTexture nativeYTex{};
    nativeYTex.object = reinterpret_cast<quint64>((__bridge void*)mtlYTexture);
    nativeYTex.layout = 0;

    QRhiTexture::NativeTexture nativeUVTex{};
    nativeUVTex.object = reinterpret_cast<quint64>((__bridge void*)mtlUVTexture);
    nativeUVTex.layout = 0;

    // 重新包装原生纹理到 QRhiTexture
    // createFrom 会替换底层纹理，QRhiTexture 对象本身保持不变
    if (!m_yTexture->createFrom(nativeYTex)) {
        NEAPU_LOGE("Failed to wrap Y texture from native Metal texture");
        return;
    }
    
    if (!m_uvTexture->createFrom(nativeUVTex)) {
        NEAPU_LOGE("Failed to wrap UV texture from native Metal texture");
        return;
    }
}

void* MetalPipeline::metalYTexture() const
{
    if (m_metalTextureY) {
        id<MTLTexture> mtlTexture = CVMetalTextureGetTexture((CVMetalTextureRef)m_metalTextureY);
        return (__bridge void*)mtlTexture;
    }
    return nullptr;
}

void* MetalPipeline::metalUVTexture() const
{
    if (m_metalTextureUV) {
        id<MTLTexture> mtlTexture = CVMetalTextureGetTexture((CVMetalTextureRef)m_metalTextureUV);
        return (__bridge void*)mtlTexture;
    }
    return nullptr;
}

bool MetalPipeline::createSrb(const QSize& size)
{
    NEAPU_FUNC_TRACE;

    // 创建空的 QRhiTexture，纹理内容会在 updateTexture 中通过 Metal 纹理填充
    // 类似 VAAPI 的做法，创建纹理但不立即分配内容
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
        NEAPU_LOGE("Unsupported format for SRB creation: {}", static_cast<int>(m_swFormat));
        return false;
    }

    // 先创建空纹理，实际内容在 updateTexture 中通过 createFrom 绑定
    if (!m_yTexture->create() || !m_uvTexture->create()) {
        NEAPU_LOGE("Failed to create initial QRhiTextures");
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

QString MetalPipeline::getFragmentShaderName()
{
    if (m_swFormat == media::Frame::PixelFormat::NV12) {
        return QStringLiteral(":/shaders/nv12.frag.qsb");
    } else if (m_swFormat == media::Frame::PixelFormat::P010) {
        return QStringLiteral(":/shaders/p010.frag.qsb");
    }
    return Pipeline::getFragmentShaderName();
}

} // namespace view
#endif
