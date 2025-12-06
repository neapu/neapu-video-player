//
// Created by liu86 on 2025/11/15.
//

#include "VideoRenderer.h"
#include <logger.h>
#include <QFile>
#include <QThreadPool>
#include "../media/Packet.h"
#include "../media/Player.h"
#ifdef __linux__
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

using media::Frame;

namespace view {
static const float vertexData[] = {
    // 位置         // 纹理坐标
    -1.0f,  1.0f,  0.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 0.0f,
    -1.0f, -1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 1.0f
};



VideoRenderer::VideoRenderer(QWidget* parent)
    : QRhiWidget(parent)
{
}
VideoRenderer::~VideoRenderer()
{
    // 必须在 QRhiWidget 析构之前释放 pipeline，因为 pipeline 中的资源依赖于 QRhi
    m_pipeline.reset();
    m_vertexBuffer.reset();
}
void VideoRenderer::initialize(QRhiCommandBuffer* cb)
{
    NEAPU_FUNC_TRACE;
    if (m_rhi != rhi()) {
        m_rhi = rhi();
        m_pipeline.reset();
    }

    if (m_pipeline) {
        return;
    }

    NEAPU_LOGI("Creating video renderer resources");

#ifdef _WIN32
    if (m_rhi->backend() == QRhi::D3D11) {
        auto* nativeHandle = reinterpret_cast<const QRhiD3D11NativeHandles*>(m_rhi->nativeHandles());
        if (!nativeHandle) {
            NEAPU_LOGE("Failed to get D3D11 native handles from QRhi");
            return;
        }

        m_d3d11Device = static_cast<ID3D11Device*>(nativeHandle->dev);
        m_d3d11DeviceContext = static_cast<ID3D11DeviceContext*>(nativeHandle->context);

        Microsoft::WRL::ComPtr<ID3D10Multithread> mt;
        if (SUCCEEDED(m_d3d11Device->QueryInterface(__uuidof(ID3D10Multithread), reinterpret_cast<void**>(mt.GetAddressOf())))) {
            mt->SetMultithreadProtected(TRUE);
        } else {
            NEAPU_LOGE("Failed to enable multithread protection on D3D11 device");
            return;
        }

        NEAPU_LOGI("Obtained D3D11 device and context from QRhi");
    } else {
        NEAPU_LOGW("QRhi backend is not D3D11, current backend: {}", m_rhi->backendName());
    }
#endif
#ifdef __linux__
    if (m_rhi->backend() == QRhi::OpenGLES2) {
        m_eglDisplay = eglGetCurrentDisplay();
        if (m_eglDisplay == EGL_NO_DISPLAY) {
            NEAPU_LOGE("Failed to get current EGL display.");
            return;
        }
    }
#endif
    NEAPU_LOGI_STREAM << "Using QRhi backend: " << m_rhi->backendName();

    // 创建顶点缓冲区
    m_vertexBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Immutable,
        QRhiBuffer::VertexBuffer, sizeof(vertexData)));
    if (!m_vertexBuffer->create()) {
        NEAPU_LOGE("Failed to create vertex buffer for video renderer");
        m_vertexBuffer.reset();
        return;
    }

    m_pipeline = std::make_unique<Pipeline>(m_rhi);
    if (!m_pipeline->initialize(renderTarget())) {
        NEAPU_LOGE("Failed to create pipeline resources for video renderer");
        m_pipeline.reset();
        return;
    }

    // 上传顶点数据
    auto* rub = m_rhi->nextResourceUpdateBatch();
    rub->uploadStaticBuffer(m_vertexBuffer.get(), vertexData);
    cb->resourceUpdate(rub);

    emit initialized();
}
void VideoRenderer::render(QRhiCommandBuffer* cb)
{
    if (!m_pipeline) {
        return;
    }
    if (m_running) {
        RenderFrame(cb);
        update();
    } else {
        RenderEmpty(cb);
    }
}
void VideoRenderer::releaseResources()
{
    m_pipeline.reset();
    m_vertexBuffer.reset();
    QRhiWidget::releaseResources();
}
void VideoRenderer::start(double fps, int64_t startTimeUs)
{
    NEAPU_FUNC_TRACE;
    m_running = true;
    update();
}
void VideoRenderer::stop()
{
    NEAPU_FUNC_TRACE;
    m_running = false;
    update();
}

bool VideoRenderer::useD3D11() const
{
    return m_rhi->backend() == QRhi::D3D11;
}

bool VideoRenderer::useOpenGL() const
{
    return m_rhi->backend() == QRhi::OpenGLES2;
}

#ifdef _WIN32
ID3D11Device* VideoRenderer::getD3D11Device()
{
    return m_d3d11Device;
}
#endif

void VideoRenderer::RenderFrame(QRhiCommandBuffer* cb)
{
    auto frame = media::Player::instance().getVideoFrame();
    if (!frame) {
        return;
    }
    QSize frameSize{frame->width(), frame->height()};
    if (!m_pipeline->checkFormat(frame)) {
        m_pipeline.reset();
        Pipeline::CreateParam param;
        param.rhi = m_rhi;
#ifdef _WIN32
        param.d3d11Device = m_d3d11Device;
        param.d3d11DeviceContext = m_d3d11DeviceContext;
#endif
#ifdef __linux__
        param.eglDisplay = m_eglDisplay;
#endif
        m_pipeline = Pipeline::makeFormFrame(frame, param);
        if (!m_pipeline) {
            NEAPU_LOGE("Failed to create pipeline for frame rendering");
            return;
        }
        if (!m_pipeline->initialize(renderTarget(), frameSize)) {
            NEAPU_LOGE("Failed to create pipeline resources for frame rendering");
            m_pipeline.reset();
            return;
        }
    }

    const QSize pixelSize = renderTarget()->pixelSize();
    auto rub = m_rhi->nextResourceUpdateBatch();

    m_pipeline->updateColorParamsIfNeeded(frame, rub);
    m_pipeline->updateTexture(rub, std::move(frame));
    m_pipeline->updateVertexUniforms(rub, pixelSize, frameSize);

    RenderImpl(rub, cb);
}
void VideoRenderer::RenderEmpty(QRhiCommandBuffer* cb)
{
    if (!m_pipeline || m_pipeline->pixelFormat() != media::Frame::PixelFormat::None) {
        NEAPU_LOGI("Creating pipeline for empty frame rendering");
        m_pipeline.reset();
        m_pipeline = std::make_unique<Pipeline>(m_rhi);
        if (!m_pipeline->initialize(renderTarget())) {
            NEAPU_LOGE("Failed to create pipeline resources for empty frame rendering");
            m_pipeline.reset();
            return;
        }
    }

    auto rub = m_rhi->nextResourceUpdateBatch();

    RenderImpl(rub, cb);
}
void VideoRenderer::RenderImpl(QRhiResourceUpdateBatch* rub, QRhiCommandBuffer* cb) const
{
    const QSize pixelSize = renderTarget()->pixelSize();
    cb->beginPass(renderTarget(), QColor(0, 0, 0, 255), {1.0f, 0}, rub);

    cb->setGraphicsPipeline(m_pipeline->getPipeline());
    cb->setShaderResources(m_pipeline->getSrb());
    cb->setViewport(QRhiViewport(0.0f, 0.0f, static_cast<float>(pixelSize.width()), static_cast<float>(pixelSize.height())));

    const QRhiCommandBuffer::VertexInput vertexInput[] = {{m_vertexBuffer.get(), 0}};
    cb->setVertexInput(0, 1, vertexInput);
    cb->draw(4);
    cb->endPass();
}

} // namespace view