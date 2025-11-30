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
static int64_t getCurrentTimeUs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
static const float vertexData[] = {
    // 位置         // 纹理坐标
    -1.0f,  1.0f,  0.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 0.0f,
    -1.0f, -1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 1.0f
};

static QShader loadShader(const QString& name)
{
    QFile file(name);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open shader file:" << name;
        return {};
    }
    return QShader::fromSerialized(file.readAll());
}

VideoRenderer::VideoRenderer(QWidget* parent)
    : QRhiWidget(parent)
{
}
VideoRenderer::~VideoRenderer()
{
    NEAPU_FUNC_TRACE;
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

    // 创建采样器
    m_sampler.reset(m_rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
    if (!m_sampler->create()) {
        NEAPU_LOGE("Failed to create sampler for video renderer");
        m_sampler.reset();
        return;
    }

    // 创建uniform缓冲区，传递缩放矩阵(mat4/64bytes)
    m_vsUBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64));
    if (!m_vsUBuffer->create()) {
        NEAPU_LOGE("Failed to create vertex shader uniform buffer");
        m_vsUBuffer.reset();
        return;
    }

    // 先重建一个空的管线，等待第一帧到来时再重建
    if (!createEmptyPipeline()) {
        NEAPU_LOGE("Failed to create empty pipeline for video renderer");
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
        NEAPU_LOGW("Video pipeline is not created yet");
        return;
    }

    if (m_pause) {
        return;
    }

    RenderImpl(cb);
    if (m_running) {
        update();
    }
}
void VideoRenderer::start(double fps, int64_t startTimeUs)
{
    NEAPU_FUNC_TRACE;
    m_fps = fps;
    m_running = true;
    m_pause = false;
    m_startTimeUs = startTimeUs;
    update();
}
void VideoRenderer::stop()
{
    NEAPU_FUNC_TRACE;
    m_running = false;
    m_pause = false;
    m_renderFrame.reset();
    m_startTimeUs = 0;
    m_serial = 0;
    update();
}

void VideoRenderer::pause()
{
    m_pause = true;
}

void VideoRenderer::seek(int serial)
{
    NEAPU_LOGD("Seeking video renderer to serial {}", serial);
    m_seeking = true;
    m_serial = serial;
}
int64_t VideoRenderer::currentPtsUs() const
{
    return m_currentPtsUs;
}
#ifdef _WIN32
ID3D11Device* VideoRenderer::getD3D11Device()
{
    return m_d3d11Device;
}
#endif
void VideoRenderer::onAudioPtsUpdated(int64_t ptsUs)
{
    m_startTimeUs = getCurrentTimeUs() - ptsUs;
}
void VideoRenderer::RenderImpl(QRhiCommandBuffer* cb)
{
    if (!m_running) {
        m_renderFrame.reset();
        createEmptyPipeline();
    } else {
        if (!m_renderFrame) {
            m_renderFrame = getNextFrame();
        }
        if (!m_renderFrame) {
            return;
        }
        if (!shouldRenderNewFrame()) {
            return;
        }
        if (m_renderFrame->serial() < m_serial) {
            m_renderFrame.reset();
            return;
        }
        if (m_renderFrame->width() != m_width ||
            m_renderFrame->height() != m_height ||
            m_renderFrame->pixelFormat() != m_pixelFormat) {
            m_width = m_renderFrame->width();
            m_height = m_renderFrame->height();
            m_pixelFormat = m_renderFrame->pixelFormat();
            if (!recreatePipeline()) {
                NEAPU_LOGE("Failed to recreate pipeline for new video frame");
                return;
            }
        }
    }


    const QSize pixelSize = renderTarget()->pixelSize();
    auto rub = m_rhi->nextResourceUpdateBatch();

    updateTextures(rub);
    updateVertexUniformBuffer(rub, pixelSize);
    m_renderFrame.reset();

    cb->beginPass(renderTarget(), QColor(0, 0, 0, 255), {1.0f, 0}, rub);

    cb->setGraphicsPipeline(m_pipeline.get());
    cb->setShaderResources(m_srb.get());
    cb->setViewport(QRhiViewport(0.0f, 0.0f, static_cast<float>(pixelSize.width()), static_cast<float>(pixelSize.height())));

    const QRhiCommandBuffer::VertexInput vertexInput[] = {{m_vertexBuffer.get(), 0}};
    cb->setVertexInput(0, 1, vertexInput);
    cb->draw(4);
    cb->endPass();
}
bool VideoRenderer::recreatePipeline()
{
    if (!recreateTextures()) {
        return false;
    }

    if (!createPipeline()) {
        return false;
    }

    return true;
}

bool VideoRenderer::createEmptyPipeline()
{
    NEAPU_FUNC_TRACE;
    m_renderFrame.reset();
    m_srb.reset(m_rhi->newShaderResourceBindings());
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(3, QRhiShaderResourceBinding::VertexStage, m_vsUBuffer.get()),
    });
    if (!m_srb->create()) {
        NEAPU_LOGE("Failed to create empty shader resource bindings");
        return false;
    }

    return createPipeline();
}
bool VideoRenderer::createPipeline()
{
    NEAPU_FUNC_TRACE;
    auto vs = loadShader(":/shaders/video.vert.qsb");
    auto fs = loadShader(getFragmentShaderName());
    if (!vs.isValid() || !fs.isValid()) {
        NEAPU_LOGE("Failed to load shaders for video pipeline");
        return false;
    }

    // 顶点布局
    QRhiVertexInputLayout layout{};
    // 每个顶点包含4个float：位置(x,y)和纹理坐标(u,v)
    layout.setBindings({4*sizeof(float)});
    layout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, 0},               // 位置
        {0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)} // 纹理坐标
    });

    m_pipeline.reset(m_rhi->newGraphicsPipeline());
    m_pipeline->setShaderStages({
        {QRhiShaderStage::Vertex, vs},
        {QRhiShaderStage::Fragment, fs}
    });
    m_pipeline->setVertexInputLayout(layout);
    // 条带绘制，使用4个顶点绘制两个三角形
    m_pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    m_pipeline->setShaderResourceBindings(m_srb.get());
    // 默认混合和深度测试
    m_pipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    if (!m_pipeline->create()) {
        NEAPU_LOGE("Failed to create graphics pipeline for video rendering");
        m_pipeline.reset();
        return false;
    }
    return true;
}

bool VideoRenderer::recreateTextures()
{
    if (m_renderFrame->pixelFormat() == Frame::PixelFormat::D3D11Texture2D) {
        return createD3D11Texture();
    } else {
        return createYUVTextures();
    }
}
bool VideoRenderer::createYUVTextures()
{
    NEAPU_FUNC_TRACE;
    int width = m_renderFrame->width();
    int height = m_renderFrame->height();
    m_textures[0].reset(m_rhi->newTexture(QRhiTexture::R8,
        QSize(width, height), 1, QRhiTexture::Flags()));
    m_textures[1].reset(m_rhi->newTexture(QRhiTexture::R8,
        QSize((width + 1) / 2, (height + 1) / 2), 1, QRhiTexture::Flags()));
    m_textures[2].reset(m_rhi->newTexture(QRhiTexture::R8,
        QSize((width + 1) / 2, (height + 1) / 2), 1, QRhiTexture::Flags()));
    if (!m_textures[0]->create() || !m_textures[1]->create() || !m_textures[2]->create()) {
        NEAPU_LOGE("Failed to create YUV textures");
        m_textures[0].reset();
        m_textures[1].reset();
        m_textures[2].reset();
        return false;
    }

    // 重建着色器绑定
    m_srb.reset(m_rhi->newShaderResourceBindings());
    m_srb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, m_textures[0].get(), m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_textures[1].get(), m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, m_textures[2].get(), m_sampler.get()),
        QRhiShaderResourceBinding::uniformBuffer(3, QRhiShaderResourceBinding::VertexStage, m_vsUBuffer.get()),
    });
    if (!m_srb->create()) {
        NEAPU_LOGE("Failed to create shader resource bindings for YUV textures");
        m_textures[0].reset();
        m_textures[1].reset();
        m_textures[2].reset();
        return false;
    }

    return true;
}
bool VideoRenderer::createD3D11Texture()
{
    NEAPU_FUNC_TRACE;
#ifdef _WIN32
    if (!m_renderFrame || m_renderFrame->pixelFormat() != Frame::PixelFormat::D3D11Texture2D) {
        NEAPU_LOGE("Current frame is not D3D11 texture");
        return false;
    }

    if (!m_d3d11Device) {
        NEAPU_LOGE("D3D11 device is null");
        return false;
    }

    auto* srcTexture = m_renderFrame->d3d11Texture2D();
    if (!srcTexture) {
        NEAPU_LOGE("D3D11 texture is null in current frame");
        return false;
    }
    D3D11_TEXTURE2D_DESC srcDesc{};
    srcTexture->GetDesc(&srcDesc);
    if (srcDesc.Format != DXGI_FORMAT_NV12 && srcDesc.Format != DXGI_FORMAT_P010) {
        NEAPU_LOGE("Unsupported D3D11 texture format: {}", static_cast<int>(srcDesc.Format));
        return false;
    }

    // 创建原生纹理
    m_nativeTexture.Reset();
    D3D11_TEXTURE2D_DESC desc = {};
    // 这里使用视频帧的尺寸创建纹理，因为解码出来的纹理可能是对齐填充过的，会比实际视频尺寸大
    desc.Width = m_renderFrame->width();
    desc.Height = m_renderFrame->height();
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = srcDesc.Format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    auto hr = m_d3d11Device->CreateTexture2D(&desc, nullptr, m_nativeTexture.GetAddressOf());
    if (FAILED(hr)) {
        NEAPU_LOGE("Failed to create D3D11 texture: HRESULT={}", hr);
        return false;
    }

    // 创建QRhi包装纹理
    QRhiTexture::NativeTexture nativeTex{};
    nativeTex.object = reinterpret_cast<quint64>(m_nativeTexture.Get());
    nativeTex.layout = 0; // d3d11不需要

    auto width = m_renderFrame->width();
    auto height = m_renderFrame->height();
    if (desc.Format == DXGI_FORMAT_NV12) {
        m_textures[0].reset(m_rhi->newTexture(QRhiTexture::R8,
            QSize(width, height), 1, QRhiTexture::Flags()));
        m_textures[1].reset(m_rhi->newTexture(QRhiTexture::RG8,
            QSize((width + 1) / 2, (height + 1) / 2), 1, QRhiTexture::Flags()));
    } else if (desc.Format == DXGI_FORMAT_P010) {
        m_textures[0].reset(m_rhi->newTexture(QRhiTexture::R16,
            QSize(width, height), 1, QRhiTexture::Flags()));
        m_textures[1].reset(m_rhi->newTexture(QRhiTexture::RG16,
            QSize((width + 1) / 2, (height + 1) / 2), 1, QRhiTexture::Flags()));
    } else {
        NEAPU_LOGE("Unsupported D3D11 texture format for QRhi: {}", static_cast<int>(desc.Format));
        m_nativeTexture.Reset();
        return false;
    }

    // 从原生纹理创建QRhi纹理，QRhi纹理只是包装，不会复制数据
    if (!m_textures[0]->createFrom(nativeTex) || !m_textures[1]->createFrom(nativeTex)) {
        NEAPU_LOGE("Failed to create QRhi textures from D3D11 native texture");
        m_textures[0].reset();
        m_textures[1].reset();
        m_nativeTexture.Reset();
        return false;
    }

    // 重建着色器绑定
    m_srb.reset(m_rhi->newShaderResourceBindings());
    m_srb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, m_textures[0].get(), m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_textures[1].get(), m_sampler.get()),
        QRhiShaderResourceBinding::uniformBuffer(3, QRhiShaderResourceBinding::VertexStage, m_vsUBuffer.get()),
    });
    if (!m_srb->create()) {
        NEAPU_LOGE("Failed to create shader resource bindings for D3D11 texture");
        m_textures[0].reset();
        m_textures[1].reset();
        m_nativeTexture.Reset();
        return false;
    }

    return true;
#endif
    return false;
}
void VideoRenderer::updateTextures(QRhiResourceUpdateBatch* rub)
{
    if (!m_renderFrame) {
        NEAPU_LOGW("Current frame is null when updating textures");
        return;
    }
    // NEAPU_LOGD("Render pts={}us", m_renderFrame->ptsUs());
    m_currentPtsUs = m_renderFrame->ptsUs();
    emit playingPts(m_renderFrame->ptsUs());
    if (m_renderFrame->pixelFormat() == Frame::PixelFormat::D3D11Texture2D) {
        updateD3D11Texture();
    } else {
        updateYUVTextures(rub);
    }
}

void VideoRenderer::updateYUVTextures(QRhiResourceUpdateBatch* rub)
{
    if (!m_renderFrame) {
        NEAPU_LOGE("Current frame is null when updating YUV textures");
        return;
    }

    if (m_renderFrame->pixelFormat() != Frame::PixelFormat::YUV420P) {
        NEAPU_LOGE("Current frame is not YUV420P when updating YUV textures");
        return;
    }

    // 判断是否有填充，如果有填充需要逐行复制，如果没有填充，可以一次性上传
    bool needPerLineCopy = false;
    int width = m_renderFrame->width();
    int height = m_renderFrame->height();
    int yLineSize = m_renderFrame->yLineSize();
    int uLineSize = m_renderFrame->uLineSize();
    int vLineSize = m_renderFrame->vLineSize();
    if (yLineSize != width || uLineSize != (width + 1) / 2 || vLineSize != (width + 1) / 2) {
        needPerLineCopy = true;
    }
    QByteArray data[3];
    if (needPerLineCopy) {
        // 逐行复制
        data[0].resize(width * height);
        data[1].resize(((width + 1) / 2) * ((height + 1) / 2));
        data[2].resize(((width + 1) / 2) * ((height + 1) / 2));
        uint8_t* yPtr = reinterpret_cast<uint8_t*>(data[0].data());
        uint8_t* uPtr = reinterpret_cast<uint8_t*>(data[1].data());
        uint8_t* vPtr = reinterpret_cast<uint8_t*>(data[2].data());
        // 复制Y平面
        for (int row = 0; row < height; ++row) {
            std::memcpy(yPtr + row * width,
                m_renderFrame->yData() + row * yLineSize,
                width);
        }
        // 复制U平面和V平面
        for (int row = 0; row < (height + 1) / 2; ++row) {
            std::memcpy(uPtr + row * ((width + 1) / 2),
                m_renderFrame->uData() + row * uLineSize,
                (width + 1) / 2);
            std::memcpy(vPtr + row * ((width + 1) / 2),
                m_renderFrame->vData() + row * vLineSize,
                (width + 1) / 2);
        }
    } else {
        // 一次性上传
        data[0] = QByteArray(reinterpret_cast<const char*>(m_renderFrame->yData()), width * height);
        data[1] = QByteArray(reinterpret_cast<const char*>(m_renderFrame->uData()), ((width + 1) / 2) * ((height + 1) / 2));
        data[2] = QByteArray(reinterpret_cast<const char*>(m_renderFrame->vData()), ((width + 1) / 2) * ((height + 1) / 2));
    }

    for (int i = 0; i < 3; i++) {
        QRhiTextureSubresourceUploadDescription sub(data[i]);
        if (i == 0) {
            sub.setSourceSize(QSize(width, height));
        } else {
            sub.setSourceSize(QSize((width + 1) / 2, (height + 1) / 2));
        }
        QRhiTextureUploadEntry entry(0, 0, sub);
        QRhiTextureUploadDescription upload({entry});
        rub->uploadTexture(m_textures[i].get(), upload);
    }
}

void VideoRenderer::updateD3D11Texture()
{
#ifdef _WIN32
    D3D11_BOX srcBox{};
    srcBox.left = 0;
    srcBox.top = 0;
    srcBox.front = 0;
    srcBox.right = m_renderFrame->width();
    srcBox.bottom = m_renderFrame->height();
    srcBox.back = 1;

    auto* srcTexture = m_renderFrame->d3d11Texture2D();
    if (!srcTexture) {
        NEAPU_LOGE("D3D11 texture is null in current frame");
        return;
    }

    m_d3d11DeviceContext->CopySubresourceRegion(
        m_nativeTexture.Get(),
        0, 0, 0, 0,
        srcTexture,
        m_renderFrame->subresourceIndex(),
        &srcBox);
#else
    NEAPU_LOGE("D3D11 texture update called on non-Windows platform");
#endif
}
void VideoRenderer::updateVertexUniformBuffer(QRhiResourceUpdateBatch* rub, QSize renderSize)
{
    if (!m_renderFrame) {
        NEAPU_LOGW("Current frame is null when updating vertex uniform buffer");
        return;
    }
    QMatrix4x4 mvp;
    mvp.setToIdentity();

    // 根据窗口和视频帧尺寸计算缩放比例，保持宽高比
    float scaleX = 1.0f;
    float scaleY = 1.0f;

    int videoW = m_renderFrame->width();
    int videoH = m_renderFrame->height();
    if (m_lastVideoSize.width() == videoW &&
        m_lastVideoSize.height() == videoH &&
        m_lastRenderSize == renderSize) {
        // 尺寸未变化，无需更新
        return;
    }

    m_lastVideoSize = QSize(videoW, videoH);
    m_lastRenderSize = renderSize;

    if (renderSize.width() > 0 && renderSize.height() > 0 && videoW > 0 && videoH > 0) {
        const float winRatio = static_cast<float>(renderSize.width()) / static_cast<float>(renderSize.height());
        const float videoRatio = static_cast<float>(videoW) / static_cast<float>(videoH);

        if (winRatio > videoRatio) {
            // 窗口更宽，压缩 X 方向以适配高度
            scaleX = videoRatio / winRatio;
            scaleY = 1.0f;
        } else if (winRatio < videoRatio) {
            // 窗口更窄（更高），压缩 Y 方向以适配宽度
            scaleX = 1.0f;
            scaleY = winRatio / videoRatio;
        } else {
            scaleX = 1.0f;
            scaleY = 1.0f;
        }
    }

    NEAPU_LOGD("Update mvp matrix. scaleX={}, scaleY={}", scaleX, scaleY);
    mvp.scale(scaleX, scaleY);

    rub->updateDynamicBuffer(m_vsUBuffer.get(), 0, 64, mvp.constData());
}
QString VideoRenderer::getFragmentShaderName()
{
    if (!m_renderFrame) {
        return ":/shaders/none.frag.qsb";
    }

    QString name;
    using enum Frame::PixelFormat;
    if (m_renderFrame->pixelFormat() == YUV420P) {
        name = "yuv420p";
    } else if (m_renderFrame->pixelFormat() == D3D11Texture2D) {
#ifdef _WIN32
        D3D11_TEXTURE2D_DESC desc;
        auto* texture = m_renderFrame->d3d11Texture2D();
        if (!texture) {
            NEAPU_LOGE("D3D11 texture is null in current frame");
            return ":/shaders/none.frag.qsb";
        }
        texture->GetDesc(&desc);
        if (desc.Format == DXGI_FORMAT_NV12) {
            name = "nv12";
        } else if (desc.Format == DXGI_FORMAT_P010) {
            name = "p010";
        } else {
            NEAPU_LOGE("Unsupported D3D11 texture format: {}", static_cast<int>(desc.Format));
            return ":/shaders/none.frag.qsb";
        }
#else
        return ":/shaders/none.frag.qsb";
#endif
    } else {
        return ":/shaders/none.frag.qsb";
    }

    if (m_renderFrame->colorRange() == Frame::ColorRange::Full) {
        name += "_full";
    } else {
        name += "_limited";
    }

    if (m_renderFrame->colorSpace() == Frame::ColorSpace::BT709) {
        name += "_bt709";
    } else {
        name += "_bt601";
    }

    return QString(":/shaders/%1.frag.qsb").arg(name);
}
media::FramePtr VideoRenderer::getNextFrame()
{
    // QThreadPool::globalInstance()->start([this]() {
        // while (m_running) {
        //     auto nextFrame = media::Player::instance().getVideoFrame();
        //     if (!nextFrame) {
        //         return;
        //     }
        //     if (nextFrame->type() == Frame::FrameType::EndOfStream) {
        //         // 收到结束帧
        //         NEAPU_LOGI("Received end-of-file video frame");
        //         emit eof();
        //         return;
        //     }
        //
        //     if (nextFrame->serial() < m_serial) {
        //         // 丢弃过期帧
        //         NEAPU_LOGD("Discarding expired video frame with serial {}, current serial is {}",
        //             nextFrame->serial(), m_serial.load());
        //         continue;
        //     }
        //     if (nextFrame->type() == Frame::FrameType::Flush) {
        //         m_startTimeUs = 0;
        //         m_seeking = false;
        //         NEAPU_LOGD("Received flush video frame, resetting start time");
        //         continue;
        //     }
        //     if (m_startTimeUs == 0) {
        //         // 这个值为0代表是刚seek过，需要重新计算startTimeUs
        //         m_startTimeUs = getCurrentTimeUs() - nextFrame->ptsUs();
        //         m_nextFrame = std::move(nextFrame);
        //         update();
        //         return;
        //     }
        //     auto expectedPlayTimeUs = getCurrentTimeUs() - m_startTimeUs;
        //
        //     // 如果pts早于预播放时间，并且超过了阈值，则丢弃
        //     auto thresholdUs = static_cast<int64_t>(1e6 / m_fps);
        //     if (nextFrame->ptsUs() < expectedPlayTimeUs - thresholdUs) {
        //         NEAPU_LOGD("Discarding late video frame with PTS {}us, expected play time {}us",
        //             nextFrame->ptsUs(), expectedPlayTimeUs);
        //         continue;
        //     }
        //     // 如果pts晚与预播放时间，说明还没到播放时间，需要等待
        //     int64_t waitTimeUs = nextFrame->ptsUs() - expectedPlayTimeUs;
        //     if (waitTimeUs > 1000000) {
        //         // 等待超过1秒，不正常
        //         NEAPU_LOGE("Unrealistic wait time for video frame: {}us", waitTimeUs);
        //         waitTimeUs = 1000000;
        //     }
        //     if (waitTimeUs > 0) {
        //         // NEAPU_LOGD("Waiting {}us for video frame with PTS {}us", waitTimeUs, nextFrame->ptsUs());
        //         QThread::usleep(static_cast<unsigned long>(waitTimeUs));
        //     }
        //     m_nextFrame = std::move(nextFrame);
        //     update();
        //     return;
        // }
    // });
    while (m_running) {
        auto nextFrame = media::Player::instance().getVideoFrame();
        if (!nextFrame) {
            return nullptr;
        }
        if (nextFrame->type() == Frame::FrameType::EndOfStream) {
            // 收到结束帧
            NEAPU_LOGI("Received end-of-file video frame");
            emit eof();
            return nullptr;
        }
        if (nextFrame->serial() < m_serial) {
            // 丢弃过期帧
            NEAPU_LOGD("Discarding expired video frame with serial {}, current serial is {}",
                nextFrame->serial(), m_serial.load());
            continue;
        }
        if (nextFrame->type() == Frame::FrameType::Flush) {
            m_startTimeUs = 0;
            m_seeking = false;
            NEAPU_LOGD("Received flush video frame, resetting start time");
            continue;
        }
        return nextFrame;
    }
    return nullptr;
}
bool VideoRenderer::shouldRenderNewFrame()
{
    if (!m_renderFrame) {
        return false;
    }

    auto expectedPlayTimeUs = getCurrentTimeUs() - m_startTimeUs;
    return m_renderFrame->ptsUs() <= expectedPlayTimeUs;
}

} // namespace view