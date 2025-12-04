//
// Created by liu86 on 2025/11/15.
//

#include "VideoRenderer.h"
#include <logger.h>
#include <QFile>
#include <QThreadPool>
#include "../media/Packet.h"
#include "../media/Player.h"

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
    auto backendName = m_rhi->backendName();
    NEAPU_LOGI("QRhi backend: {}", backendName);

    // 创建顶点缓冲区
    m_vertexBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Immutable,
        QRhiBuffer::VertexBuffer, sizeof(vertexData)));
    if (!m_vertexBuffer->create()) {
        NEAPU_LOGE("Failed to create vertex buffer for video renderer");
        m_vertexBuffer.reset();
        return;
    }

    m_pipeline = Pipeline::createForFrame(nullptr, m_rhi, renderTarget());
    if (!m_pipeline) {
        NEAPU_LOGE("Failed to create pipeline for video renderer");
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
    if (!m_pipeline->checkFormat(frame)) {
        m_pipeline.reset();
#ifdef _WIN32
        m_pipeline = Pipeline::createForFrame(frame, m_rhi, renderTarget(), m_d3d11Device, m_d3d11DeviceContext);
#else
        m_pipeline = Pipeline::createForFrame(frame, m_rhi, renderTarget());
#endif
        if (!m_pipeline) {
            NEAPU_LOGE("Failed to create pipeline for frame rendering");
            return;
        }
    }

    const QSize pixelSize = renderTarget()->pixelSize();
    auto rub = m_rhi->nextResourceUpdateBatch();

    m_pipeline->updateColorParamsIfNeeded(frame, rub);
    m_pipeline->updateTexture(rub, std::move(frame));
    m_pipeline->updateVertexUniforms(rub, pixelSize);

    RenderImpl(rub, cb);
}
void VideoRenderer::RenderEmpty(QRhiCommandBuffer* cb)
{
    m_pipeline.reset();
    m_pipeline = Pipeline::createForFrame(nullptr, m_rhi, renderTarget());
    if (!m_pipeline) {
        NEAPU_LOGE("Failed to create pipeline for empty frame rendering");
        return;
    }


    auto rub = m_rhi->nextResourceUpdateBatch();

    RenderImpl(rub, cb);
}
void VideoRenderer::RenderImpl(QRhiResourceUpdateBatch* rub, QRhiCommandBuffer* cb)
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