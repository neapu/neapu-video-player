//
// Created by neapu on 2025/12/5.
//

#include "VaapiPipeline.h"
#ifdef __linux__
#include <logger.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <va/va.h>
#include <va/va_drmcommon.h>
#include <QOpenGLContext>
#include <QOpenGLFunctions>

static PFNEGLCREATEIMAGEKHRPROC s_eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
static PFNEGLDESTROYIMAGEKHRPROC s_eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC s_glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");


namespace view {
VaapiPipeline::VaapiPipeline(QRhi* rhi, void* vaDisplay, void* eglDisplay, media::Frame::PixelFormat swFormat)
    : Pipeline(rhi)
    , m_vaDisplay(vaDisplay)
    , m_eglDisplay(eglDisplay)
{
    NEAPU_FUNC_TRACE;
    m_swFormat = swFormat;
    if (m_swFormat != media::Frame::PixelFormat::NV12 &&
        m_swFormat != media::Frame::PixelFormat::P010) {
        NEAPU_LOGE("Unsupported VAAPI software format: {}", static_cast<int>(m_swFormat));
        throw std::runtime_error("Unsupported VAAPI software format");
    }
    m_pixelFormat = media::Frame::PixelFormat::Vaapi;

    if (s_eglCreateImageKHR == nullptr ||
        s_eglDestroyImageKHR == nullptr ||
        s_glEGLImageTargetTexture2DOES == nullptr) {
        NEAPU_LOGE("Failed to get EGL extension function pointers.");
        throw std::runtime_error("Failed to get EGL extension function pointers.");
    }

    auto* nativeHandles = reinterpret_cast<const QRhiGles2NativeHandles*>(m_rhi->nativeHandles());
    const auto* openglContext = static_cast<QOpenGLContext*>(nativeHandles->context);
    m_glFuncs = openglContext->functions();
}
VaapiPipeline::~VaapiPipeline()
{
    NEAPU_FUNC_TRACE;
}

void VaapiPipeline::updateTexture(QRhiResourceUpdateBatch* rub, media::FramePtr&& frame)
{
    VASurfaceID vaSurface = frame->vaapiSurfaceId();

    // 同步确保解码完成
    VAStatus vaStatus = vaSyncSurface(m_vaDisplay, vaSurface);
    if (vaStatus != VA_STATUS_SUCCESS) {
        NEAPU_LOGE("vaSyncSurface failed: {}", static_cast<int>(vaStatus));
        return;
    }

    // 导出为 DRM PRIME
    VADRMPRIMESurfaceDescriptor primeDesc;
    vaStatus = vaExportSurfaceHandle(
        m_vaDisplay,
        vaSurface,
        VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
        VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS,
        &primeDesc
    );
    if (vaStatus != VA_STATUS_SUCCESS) {
        NEAPU_LOGE("vaExportSurfaceHandle failed: {}", static_cast<int>(vaStatus));
        return;
    }

    // 清理旧 EGL Images
    if (m_eglImageY != EGL_NO_IMAGE) {
        s_eglDestroyImageKHR(m_eglDisplay, (EGLImageKHR)m_eglImageY);
        m_eglImageY = EGL_NO_IMAGE;
    }
    if (m_eglImageUV != EGL_NO_IMAGE) {
        s_eglDestroyImageKHR(m_eglDisplay, (EGLImageKHR)m_eglImageUV);
        m_eglImageUV = EGL_NO_IMAGE;
    }

    NEAPU_LOGD_STREAM << "VAAPI surface export: num_layers=" << primeDesc.num_layers
             << ", num_objects=" << primeDesc.num_objects
             << ", frame size=" << frame->width() << "x" << frame->height()
             << ", fourcc=" << Qt::hex << primeDesc.fourcc;

    // 为每个层（Y 和 UV）创建 EGLImage
    for (uint32_t i = 0; i < primeDesc.num_layers && i < 2; ++i) {
        const auto& layer = primeDesc.layers[i];
        const auto& object = primeDesc.objects[layer.object_index[0]];

        EGLint drmFormat = static_cast<EGLint>(layer.drm_format);

        NEAPU_LOGD_STREAM << "Layer" << i << ": drm_format=" << Qt::hex << drmFormat
                 << ", pitch=" << Qt::dec << layer.pitch[0] << ", offset=" << layer.offset[0]
                 << ", num_planes=" << layer.num_planes
                 << ", object_idx=" << layer.object_index[0]
                 << ", drm_modifier=" << Qt::hex << object.drm_format_modifier;

        EGLint width, height;
        if (i == 0) { // Y 平面
            width = frame->width();
            height = frame->height();
        } else { // UV 平面
            width = frame->width() / 2;
            height = frame->height() / 2;
        }

        // 创建 EGLImage - 需要支持 DRM modifier (tile 格式)
        // clang-format off
        EGLint attribs[] = {
            EGL_LINUX_DRM_FOURCC_EXT, drmFormat,
            EGL_WIDTH, width,
            EGL_HEIGHT, height,
            EGL_DMA_BUF_PLANE0_FD_EXT, object.fd,
            EGL_DMA_BUF_PLANE0_OFFSET_EXT, static_cast<EGLint>(layer.offset[0]),
            EGL_DMA_BUF_PLANE0_PITCH_EXT, static_cast<EGLint>(layer.pitch[0]),
            EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, static_cast<EGLint>(object.drm_format_modifier & 0xFFFFFFFF),
            EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, static_cast<EGLint>(object.drm_format_modifier >> 32),
            EGL_NONE
        };
        // clang-format on

        EGLImage eglImage = s_eglCreateImageKHR(
            m_eglDisplay,
            EGL_NO_CONTEXT,
            EGL_LINUX_DMA_BUF_EXT,
            nullptr,
            attribs
        );
        if (eglImage == EGL_NO_IMAGE) {
            NEAPU_LOGE("eglCreateImageKHR failed for layer {}.", i);
            // 关闭 DRM fds
            for (uint32_t j = 0; j < primeDesc.num_objects; ++j) {
                close(primeDesc.objects[j].fd);
            }
            return;
        }

        // 绑定到 OpenGL 纹理
        GLuint texture = (i == 0) ? yGLTexture() : uvGLTexture();
        m_glFuncs->glBindTexture(GL_TEXTURE_2D, texture);
        s_glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, eglImage);

        if (i == 0) {
            m_eglImageY = eglImage;
        } else {
            m_eglImageUV = eglImage;
        }

        m_glFuncs->glBindTexture(GL_TEXTURE_2D, 0);
    }

    // 关闭 DRM fds
    for (uint32_t j = 0; j < primeDesc.num_objects; ++j) {
        close(primeDesc.objects[j].fd);
    }
}
bool VaapiPipeline::createSrb(const QSize& size)
{
    NEAPU_FUNC_TRACE;

    m_yTexture.reset(m_rhi->newTexture(QRhiTexture::R8, QSize(size.width(), size.height()), 1, QRhiTexture::Flags()));
    m_uvTexture.reset(m_rhi->newTexture(QRhiTexture::RG8, QSize(size.width() / 2, size.height() / 2), 1, QRhiTexture::Flags()));
    if (m_yTexture->create() == false ||
        m_uvTexture->create() == false) {
        NEAPU_LOGE("Failed to create QRhiTextures for VAAPI SRB creation");
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
QString VaapiPipeline::getFragmentShaderName()
{
    if (m_swFormat == media::Frame::PixelFormat::NV12) {
        return QStringLiteral(":/shaders/nv12.frag.qsb");
    }
    if (m_swFormat == media::Frame::PixelFormat::P010) {
        return QStringLiteral(":/shaders/p010.frag.qsb");
    }
    return Pipeline::getFragmentShaderName();
}
GLint VaapiPipeline::yGLTexture() const
{
    return static_cast<GLint>(m_yTexture->nativeTexture().object);
}
GLint VaapiPipeline::uvGLTexture() const
{
    return static_cast<GLint>(m_uvTexture->nativeTexture().object);
}
} // namespace view
#endif