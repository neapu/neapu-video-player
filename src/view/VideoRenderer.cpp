//
// Created by liu86 on 2025/10/28.
//

#include "VideoRenderer.h"
#include <QFile>
#include "logger.h"

namespace view {
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

void VideoRenderer::renderFrame(media::VideoFramePtr frame) 
{
    if (!frame) {
        NEAPU_LOGW("Received null video frame");
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        m_currentFrame = std::move(frame);
    }
    
    update();
}

void VideoRenderer::stop(bool flush)
{
    NEAPU_FUNC_TRACE;
    
    m_currentFrame.reset();
    m_currentWidth = 0;
    m_currentHeight = 0;
    m_currentPixelFormat = media::VideoFrame::PixelFormat::NONE;
    
    if (flush) {
        QMetaObject::invokeMethod(this, [this]() {
            createEmptyPipeline();
            update();
        }, Qt::QueuedConnection);
    }

}
void VideoRenderer::initialize(QRhiCommandBuffer* cb)
{
    NEAPU_FUNC_TRACE;
    if (m_rhi == nullptr) {
        m_rhi = rhi();
        m_pipeline.reset();
    }

    if (m_pipeline) {
        return;
    }

    NEAPU_LOGI("Initializing video renderer");

#ifdef _WIN32
    if (m_rhi->backend() == QRhi::D3D11) {
        auto* nativeHandle = reinterpret_cast<const QRhiD3D11NativeHandles*>(m_rhi->nativeHandles());
        if (nativeHandle) {
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
            NEAPU_LOGE("Failed to obtain native handles from QRhi for D3D11");
            return;
        }
    } else {
        NEAPU_LOGW("QRhi backend is not D3D11, current backend: {}", static_cast<int>(m_rhi->backend()));
    }
#endif

    // 创建顶点缓冲区
    m_vertexBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(vertexData)));
    if (!m_vertexBuffer->create()) {
        NEAPU_LOGE("Failed to create vertex buffer");
        return;
    }

    // 创建纹理采样器
    // clang-format off
    m_sampler.reset(m_rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
        QRhiSampler::None, QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
    // clang-format on
    if (!m_sampler->create()) {
        NEAPU_LOGE("Failed to create sampler");
        return;
    }

    // 创建uniform缓冲区（64字节的mat4矩阵）
    m_vsUBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64));
    if (!m_vsUBuffer->create()) {
        NEAPU_LOGE("Failed to create uniform buffer");
        return;
    }

    // 创建片段着色器的uniform缓冲区（16字节：ivec4 色彩参数）
    m_fsUBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 16));
    if (!m_fsUBuffer->create()) {
        NEAPU_LOGE("Failed to create fragment uniform buffer");
        return;
    }

    // 创建渲染管线
    if (!createEmptyPipeline()) {
        NEAPU_LOGE("Failed to create graphics pipeline");
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

    const QSize pixelSize = renderTarget()->pixelSize();
    auto rub = m_rhi->nextResourceUpdateBatch();

    if (!updateTexture(cb, rub)) {
        return;
    }

    updateVertexUniforms(cb, rub);

    cb->beginPass(renderTarget(), QColor(0, 0, 0, 255), {1.0f, 0}, rub);

    cb->setGraphicsPipeline(m_pipeline.get());
    cb->setShaderResources(m_srb.get());
    cb->setViewport(QRhiViewport(0.0f, 0.0f, static_cast<float>(pixelSize.width()), static_cast<float>(pixelSize.height())));

    const QRhiCommandBuffer::VertexInput vertexInput[] = {{m_vertexBuffer.get(), 0}};
    cb->setVertexInput(0, 1, vertexInput);
    cb->draw(4);
    cb->endPass();
}
ID3D11Device* VideoRenderer::d3d11Device() const
{
    return m_d3d11Device;
}
ID3D11DeviceContext* VideoRenderer::d3d11DeviceContext() const
{
    return m_d3d11DeviceContext;
}

bool VideoRenderer::createPipeline()
{
    NEAPU_FUNC_TRACE;
    auto vs = loadShader(":/shaders/video.vert.qsb");
    auto fs = loadShader(getFragmentShaderName());
    if (!vs.isValid() || !fs.isValid()) {
        NEAPU_LOGE("Failed to load shaders");
        return false;
    }

    // 设置顶点输入布局
    QRhiVertexInputLayout layout{};
    // 每个顶点包含4个float：位置(x,y)和纹理坐标(u,v)
    layout.setBindings({4*sizeof(float)});
    layout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, 0},               // 位置
        {0, 1, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)} // 纹理坐标
    });

    m_pipeline.reset(m_rhi->newGraphicsPipeline());
    m_pipeline->setShaderStages({{QRhiShaderStage::Vertex, vs}, {QRhiShaderStage::Fragment, fs}});
    m_pipeline->setVertexInputLayout(layout);
    // 条带绘制，用4个顶点绘制两个三角形
    m_pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    m_pipeline->setShaderResourceBindings(m_srb.get());
    // 使用默认的混合和深度测试设置
    m_pipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    if (!m_pipeline->create()) {
        NEAPU_LOGE("Failed to create graphics pipeline");
        return false;
    }
    return true;
}
bool VideoRenderer::createEmptyPipeline()
{
    NEAPU_FUNC_TRACE;
    m_srb.reset(m_rhi->newShaderResourceBindings());
    // 仅绑定vs需要的uniform缓冲区
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(3, QRhiShaderResourceBinding::VertexStage, m_vsUBuffer.get())
    });
    if (!m_srb->create()) {
        NEAPU_LOGE("Failed to create empty shader resource bindings");
        return false;
    }

    return createPipeline();
}
QString VideoRenderer::getFragmentShaderName()
{
    if (!m_currentFrame) {
        return ":/shaders/none.frag.qsb";
    }

    using enum media::VideoFrame::PixelFormat;
    if (m_currentFrame->pixelFormat() == YUV420P) {
        return ":/shaders/yuv420p.frag.qsb";
    }

    if (m_currentFrame->pixelFormat() == D3D11) {
#ifdef _WIN32
        if (!m_d3d11Texture) return ":/shaders/none.frag.qsb";
        D3D11_TEXTURE2D_DESC desc;
        m_d3d11Texture->GetDesc(&desc);
        if (desc.Format == DXGI_FORMAT_NV12) {
            return ":/shaders/nv12.frag.qsb";
        } else if (desc.Format == DXGI_FORMAT_P010) {
            return ":/shaders/p010.frag.qsb";
        } else {
            NEAPU_LOGE("Unsupported D3D11 texture format: {}", static_cast<int>(desc.Format));
            return ":/shaders/none.frag.qsb";
        }
#endif
    }

    return ":/shaders/none.frag.qsb";
}
bool VideoRenderer::updateTexture(QRhiCommandBuffer* cb, QRhiResourceUpdateBatch* rub)
{
    std::lock_guard lock(m_frameMutex);
    if (!m_currentFrame) {
        return true;
    }

    if (m_currentWidth != m_currentFrame->width() ||
        m_currentHeight != m_currentFrame->height() ||
        m_currentPixelFormat != m_currentFrame->pixelFormat()) {
        // 视频帧尺寸或格式变化，重新创建纹理和管线
        if (!createTextures()) {
            return false;
        }

        if (!createPipeline()) {
            return false;
        }

        // 色彩空间通常在换源时变化，重建时更新一次片段uniform即可
        updateFragmentUniforms(cb, rub);

        m_currentWidth = m_currentFrame->width();
        m_currentHeight = m_currentFrame->height();
        m_currentPixelFormat = m_currentFrame->pixelFormat();
        m_videoSizeChanged = true;
    }

    switch (m_currentFrame->pixelFormat()) {
    case media::VideoFrame::PixelFormat::YUV420P: return updateYUVTextures(cb, rub);
    case media::VideoFrame::PixelFormat::D3D11: return updateD3D11Texture(cb, rub);
    default: break;
    }

    return false;
}
void VideoRenderer::updateVertexUniforms(QRhiCommandBuffer* cb, QRhiResourceUpdateBatch* rub)
{
    QMatrix4x4 mvp;
    mvp.setToIdentity();

    // 根据窗口和视频帧尺寸计算缩放比例，保持宽高比
    float scaleX = 1.0f;
    float scaleY = 1.0f;

    const QSize winSize = renderTarget()->pixelSize();
    if (!m_videoSizeChanged && winSize == m_lastRenderSize) {
        return;
    }

    m_videoSizeChanged = false;
    m_lastRenderSize = winSize;

    if (winSize.width() > 0 && winSize.height() > 0) {
        int videoW = 0;
        int videoH = 0;

        {
            std::lock_guard lock(m_frameMutex);
            videoW = m_currentWidth;
            videoH = m_currentHeight;
        }

        if (videoW > 0 && videoH > 0) {
            const float winRatio = static_cast<float>(winSize.width()) / static_cast<float>(winSize.height());
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
    }

    NEAPU_LOGD("VideoRenderer::updateVertexUniforms: scaleX={}, scaleY={}", scaleX, scaleY);
    mvp.scale(scaleX, scaleY);

    rub->updateDynamicBuffer(m_vsUBuffer.get(), 0, 64, mvp.constData());
}

void VideoRenderer::updateFragmentUniforms(QRhiCommandBuffer* cb, QRhiResourceUpdateBatch* rub)
{
    struct Params { int x; int y; int z; int w; } p{m_colorSpace, m_colorRange, m_colorTransfer, 0};
    rub->updateDynamicBuffer(m_fsUBuffer.get(), 0, sizeof(Params), &p);
}
bool VideoRenderer::createTextures()
{
    if (!m_currentFrame) {
        return false;
    }

    m_colorSpace = static_cast<int>(m_currentFrame->colorSpace());
    m_colorRange = static_cast<int>(m_currentFrame->colorRange());
    m_colorTransfer = static_cast<int>(m_currentFrame->colorTransfer());

    using enum media::VideoFrame::PixelFormat;
    switch (m_currentFrame->pixelFormat()) {
    case YUV420P: return createYUVTextures();
    case D3D11: return createD3D11Texture();
    default: break;
    }

    NEAPU_LOGE("Unsupported pixel format for texture creation");

    return false;
}

bool VideoRenderer::createYUVTextures()
{
    NEAPU_FUNC_TRACE;
    m_yTexture.reset(
        m_rhi->newTexture(QRhiTexture::R8, QSize(m_currentFrame->width(), m_currentFrame->height()), 1, QRhiTexture::RenderTarget));
    if (!m_yTexture->create()) {
        NEAPU_LOGE("Failed to create Y plane texture for YUV420P");
        return false;
    }
    m_uTexture.reset(
        m_rhi->newTexture(QRhiTexture::R8, QSize(m_currentFrame->width() / 2, m_currentFrame->height() / 2), 1, QRhiTexture::RenderTarget));
    if (!m_uTexture->create()) {
        NEAPU_LOGE("Failed to create U plane texture for YUV420P");
        return false;
    }
    m_vTexture.reset(
        m_rhi->newTexture(QRhiTexture::R8, QSize(m_currentFrame->width() / 2, m_currentFrame->height() / 2), 1, QRhiTexture::RenderTarget));
    if (!m_vTexture->create()) {
        NEAPU_LOGE("Failed to create V plane texture for YUV420P");
        return false;
    }

    // 重建资源绑定
    m_srb.reset(m_rhi->newShaderResourceBindings());
    m_srb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, m_yTexture.get(), m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_uTexture.get(), m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, m_vTexture.get(), m_sampler.get()),
        QRhiShaderResourceBinding::uniformBuffer(3, QRhiShaderResourceBinding::VertexStage, m_vsUBuffer.get()),
        QRhiShaderResourceBinding::uniformBuffer(4, QRhiShaderResourceBinding::FragmentStage, m_fsUBuffer.get()),
    });
    if (!m_srb->create()) {
        NEAPU_LOGE("Failed to create shader resource bindings for YUV420P textures");
        m_yTexture.reset();
        m_uTexture.reset();
        m_vTexture.reset();
        return false;
    }

    return true;
}

bool VideoRenderer::createD3D11Texture()
{
    NEAPU_FUNC_TRACE;
#ifdef _WIN32
    if (!m_currentFrame || !m_currentFrame->getD3D11Texture()) {
        return false;
    }

    auto* srcTexture = m_currentFrame->getD3D11Texture();
    D3D11_TEXTURE2D_DESC desc;
    srcTexture->GetDesc(&desc);
    if (desc.Format != DXGI_FORMAT_NV12 && desc.Format != DXGI_FORMAT_P010) {
        NEAPU_LOGE("Unsupported D3D11 texture format: {}", static_cast<int>(desc.Format));
        return false;
    }

    m_d3d11Texture.Reset();
    HRESULT hr = m_d3d11Device->CreateTexture2D(&desc, nullptr, m_d3d11Texture.GetAddressOf());
    if (FAILED(hr)) {
        NEAPU_LOGE("Failed to create D3D11 texture: HRESULT={}", hr);
        return false;
    }

    // 创建QRhi包装纹理
    QRhiTexture::NativeTexture nativeTex{};
    nativeTex.object = reinterpret_cast<quint64>(m_d3d11Texture.Get());
    nativeTex.layout = 0; // 未使用

    if (desc.Format == DXGI_FORMAT_NV12) {
        m_yTexture.reset(m_rhi->newTexture(
            QRhiTexture::R8, QSize(m_currentFrame->width(), m_currentFrame->height()), 1,
            QRhiTexture::Flags()));
        m_uvTexture.reset(m_rhi->newTexture(
            QRhiTexture::RG8, QSize(m_currentFrame->width() / 2, m_currentFrame->height() / 2), 1,
            QRhiTexture::Flags()));
    } else if (desc.Format == DXGI_FORMAT_P010) {
        m_yTexture.reset(m_rhi->newTexture(
            QRhiTexture::R16, QSize(m_currentFrame->width(), m_currentFrame->height()), 1,
            QRhiTexture::Flags()));
        m_uvTexture.reset(m_rhi->newTexture(
            QRhiTexture::RG16, QSize(m_currentFrame->width() / 2, m_currentFrame->height() / 2), 1,
            QRhiTexture::Flags()));
    } else {
        return false;
    }

    if (!m_yTexture->createFrom(nativeTex) || !m_uvTexture->createFrom(nativeTex)) {
        NEAPU_LOGE("Failed to create QRhi D3D11 textures");
        m_yTexture.reset();
        m_uvTexture.reset();
        m_d3d11Texture.Reset();
        return false;
    }

    m_srb.reset(m_rhi->newShaderResourceBindings());
    m_srb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, m_yTexture.get(), m_sampler.get()),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_uvTexture.get(), m_sampler.get()),
        QRhiShaderResourceBinding::uniformBuffer(3, QRhiShaderResourceBinding::VertexStage, m_vsUBuffer.get()),
        QRhiShaderResourceBinding::uniformBuffer(4, QRhiShaderResourceBinding::FragmentStage, m_fsUBuffer.get()),
    });
    if (!m_srb->create()) {
        NEAPU_LOGE("Failed to create shader resource bindings for D3D11 textures");
        m_yTexture.reset();
        m_uvTexture.reset();
        m_d3d11Texture.Reset();
        return false;
    }
    return true;
#endif
    return false;
}

bool VideoRenderer::updateYUVTextures(QRhiCommandBuffer* cb, QRhiResourceUpdateBatch* rub)
{
    if (!m_currentFrame || m_currentFrame->pixelFormat() != media::VideoFrame::PixelFormat::YUV420P) {
        return false;
    }

    if (!m_yTexture || !m_uTexture || !m_vTexture) {
        NEAPU_LOGE("YUV textures are not created");
        return false;
    }

    const int w = m_currentFrame->width();
    const int h = m_currentFrame->height();
    const size_t ySize = m_currentFrame->ySize();
    const size_t uSize = m_currentFrame->uSize();
    const size_t vSize = m_currentFrame->vSize();

    const uint8_t* yData = m_currentFrame->y();
    const uint8_t* uData = m_currentFrame->u();
    const uint8_t* vData = m_currentFrame->v();
    if (!yData || !uData || !vData) {
        NEAPU_LOGE("Invalid YUV420P frame data");
        return false;
    }

    // 在VideoFrame中以及去除了行对齐，这里可以直接拷贝
    QByteArray yBuf;
    yBuf.resize(static_cast<qsizetype>(ySize));
    std::memcpy(yBuf.data(), yData, ySize);

    QByteArray uBuf;
    uBuf.resize(static_cast<qsizetype>(uSize));
    std::memcpy(uBuf.data(), uData, uSize);

    QByteArray vBuf;
    vBuf.resize(static_cast<qsizetype>(vSize));
    std::memcpy(vBuf.data(), vData, vSize);

    m_currentFrame.reset();

    {
        QRhiTextureSubresourceUploadDescription sub(yBuf);
        sub.setSourceSize(QSize(w, h));
        QRhiTextureUploadEntry entry(0, 0, sub);
        QRhiTextureUploadDescription upload({entry});
        rub->uploadTexture(m_yTexture.get(), upload);
    }
    {
        QRhiTextureSubresourceUploadDescription sub(uBuf);
        sub.setSourceSize(QSize(w/2, h/2));
        QRhiTextureUploadEntry entry(0, 0, sub);
        QRhiTextureUploadDescription upload({entry});
        rub->uploadTexture(m_uTexture.get(), upload);
    }
    {
        QRhiTextureSubresourceUploadDescription sub(vBuf);
        sub.setSourceSize(QSize(w/2, h/2));
        QRhiTextureUploadEntry entry(0, 0, sub);
        QRhiTextureUploadDescription upload({entry});
        rub->uploadTexture(m_vTexture.get(), upload);
    }

    return true;
}
bool VideoRenderer::updateD3D11Texture(QRhiCommandBuffer* cb, QRhiResourceUpdateBatch* rub)
{
#ifdef _WIN32
    if (!m_currentFrame || !m_currentFrame->getD3D11Texture()) {
        return false;
    }

    m_d3d11DeviceContext->CopyResource(m_d3d11Texture.Get(), m_currentFrame->getD3D11Texture());
    m_currentFrame.reset();

    return true;
#endif
    return false;
}

} // namespace view