//
// Created by liu86 on 2025/12/4.
//

#include "Pipeline.h"
#include <QFile>
#include <logger.h>
#include <cstring>
#include "YuvPipeline.h"
#include "D3D11VAPipeline.h"

namespace view {
static QShader loadShader(const QString& name)
{
    QFile file(name);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open shader file:" << name;
        return {};
    }
    return QShader::fromSerialized(file.readAll());
}
Pipeline::Pipeline(media::Frame::PixelFormat pixFmt, QRhi* rhi, QRhiRenderTarget* renderTarget)
    : m_pixelFormat(pixFmt), m_rhi(rhi), m_renderTarget(renderTarget)
{
}
Pipeline::~Pipeline() = default;
std::unique_ptr<Pipeline> Pipeline::createForFrame(const media::FramePtr& frame, QRhi* rhi, QRhiRenderTarget* renderTarget)
{
    std::unique_ptr<Pipeline> pipeline;
    if (frame && frame->pixelFormat() == media::Frame::PixelFormat::YUV420P) {
        pipeline = std::make_unique<YuvPipeline>(rhi, renderTarget);
    } else {
        pipeline = std::make_unique<Pipeline>(media::Frame::PixelFormat::None, rhi, renderTarget);
    }
    if (!pipeline->create(frame, nullptr)) {
        NEAPU_LOGE("Failed to create pipeline");
        return nullptr;
    }
    return pipeline;
}
#ifdef _WIN32
std::unique_ptr<Pipeline> Pipeline::createForFrame(const media::FramePtr& frame, QRhi* rhi, QRhiRenderTarget* renderTarget,
                                                   ID3D11Device* d3d11Device, ID3D11DeviceContext* d3d11DeviceContext)
{
    if (frame && frame->pixelFormat() == media::Frame::PixelFormat::D3D11Texture2D) {
        auto pipeline = std::make_unique<D3D11VAPipeline>(rhi, renderTarget, d3d11Device, d3d11DeviceContext);
        if (!pipeline->create(frame, nullptr)) {
            NEAPU_LOGE("Failed to create D3D11VA pipeline");
            return nullptr;
        }
        return pipeline;
    }
    return createForFrame(frame, rhi, renderTarget);
}
#endif
bool Pipeline::create(const media::FramePtr& frame, QRhiCommandBuffer* cb)
{
    NEAPU_FUNC_TRACE;
    Q_UNUSED(frame);
    if (!m_rhi) {
        NEAPU_LOGE("QRhi is null");
        return false;
    }
    // 顶点缩放矩阵
    m_vsUBuffer.reset(
        m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64)
    );
    if (!m_vsUBuffer->create()) {
        NEAPU_LOGE("Failed to create vertex shader uniform buffer");
        return false;
    }
    // 创建颜色转换参数的uniform缓冲区 - 只在需要时由子类创建
    // 基础 Pipeline 使用 none.frag.qsb，不需要颜色参数
    if (m_pixelFormat != media::Frame::PixelFormat::None) {
        m_colorParamsUBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64));
        if (!m_colorParamsUBuffer->create()) {
            NEAPU_LOGE("Failed to create color params uniform buffer");
            m_colorParamsUBuffer.reset();
            return false;
        }
    }
    m_sampler.reset(m_rhi->newSampler(
        QRhiSampler::Linear,
        QRhiSampler::Linear,
        QRhiSampler::None,
        QRhiSampler::ClampToEdge,
        QRhiSampler::ClampToEdge
    ));
    if (!m_sampler->create()) {
        NEAPU_LOGE("Failed to create sampler for YUV pipeline");
        m_sampler.reset();
        return false;
    }

    if (frame) {
        m_width = frame->width();
        m_height = frame->height();
    }

    if (!createSrb()) {
        NEAPU_LOGE("Failed to create shader resource bindings");
        return false;
    }

    return createPipeline();
}
QRhiGraphicsPipeline* Pipeline::getPipeline()
{
    return m_pipeline.get();
}
QRhiShaderResourceBindings* Pipeline::getSrb()
{
    return m_srb.get();
}
bool Pipeline::checkFormat(const media::FramePtr& frame) const
{
    return frame->pixelFormat() == m_pixelFormat;
}
void Pipeline::updateVertexUniforms(QRhiResourceUpdateBatch* rub, const QSize& renderSize)
{
    if (m_oldRenderSize == renderSize && m_oldFrameSize == QSize(m_width, m_height)) {
        return;
    }

    QMatrix4x4 vertexMatrix;
    vertexMatrix.setToIdentity();

    // 根据窗口和视频帧尺寸计算缩放比例，保持宽高比
    float scaleX = 1.0f;
    float scaleY = 1.0f;

    m_oldFrameSize = QSize(m_width, m_height);
    m_oldRenderSize = renderSize;

    if (renderSize.width() > 0 && renderSize.height() > 0 && m_width > 0 && m_height > 0) {
        const float winRatio = static_cast<float>(renderSize.width()) / static_cast<float>(renderSize.height());
        const float videoRatio = static_cast<float>(m_width) / static_cast<float>(m_height);

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
    vertexMatrix.scale(scaleX, scaleY);

    rub->updateDynamicBuffer(m_vsUBuffer.get(), 0, 64, vertexMatrix.constData());
}
bool Pipeline::createSrb()
{
    m_srb.reset(m_rhi->newShaderResourceBindings());
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(3, QRhiShaderResourceBinding::VertexStage, m_vsUBuffer.get()),
    });
    if (!m_srb->create()) {
        NEAPU_LOGE("Failed to create shader resource bindings");
        m_srb.reset();
        return false;
    }
    return true;
}
QString Pipeline::getFragmentShaderName()
{
    return ":/shaders/none.frag.qsb";
}
bool Pipeline::createPipeline()
{
    NEAPU_FUNC_TRACE;
    auto vs = loadShader(":/shaders/video.vert.qsb");
    auto fs = loadShader(getFragmentShaderName());
    if (!vs.isValid() || !fs.isValid()) {
        NEAPU_LOGE("Failed to load shaders");
        return false;
    }

    // 顶点布局
    QRhiVertexInputLayout inputLayout{};
    inputLayout.setBindings({
        { sizeof(float) * 4 } // 每个顶点4个float: pos(x,y) + uv(u,v)
    });
    inputLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 }, // pos
        { 0, 1, QRhiVertexInputAttribute::Float2, sizeof(float) * 2 }, // uv
    });

    m_pipeline.reset(m_rhi->newGraphicsPipeline());
    m_pipeline->setShaderStages({
        { QRhiShaderStage::Vertex, vs },
        { QRhiShaderStage::Fragment, fs },
    });
    m_pipeline->setVertexInputLayout(inputLayout);
    // 条带绘制，使用4个顶点绘制两个三角形
    m_pipeline->setTopology(QRhiGraphicsPipeline::TriangleStrip);
    m_pipeline->setShaderResourceBindings(m_srb.get());
    // 默认混合和深度测试
    m_pipeline->setRenderPassDescriptor(m_renderTarget->renderPassDescriptor());
    if (!m_pipeline->create()) {
        NEAPU_LOGE("Failed to create graphics pipeline");
        m_pipeline.reset();
        return false;
    }

    return true;
}
void Pipeline::updateColorParamsIfNeeded(const media::FramePtr& frame, QRhiResourceUpdateBatch* rub)
{
    if (!m_colorParamsUBuffer || !frame) {
        return;
    }

    // 检查颜色空间是否改变，如果没有改变且已初始化则跳过更新
    if (m_colorParamsInitialized && 
        m_colorSpace == frame->colorSpace() && 
        m_colorRange == frame->colorRange()) {
        return;
    }

    m_colorSpace = frame->colorSpace();
    m_colorRange = frame->colorRange();
    m_colorParamsInitialized = true;

    float yOffset = 0.0f;
    if (m_colorRange == media::Frame::ColorRange::Limited) {
        yOffset = 16.0f / 255.0f;
    }

    constexpr float bt601Limited[9] = {
        1.164f,  0.0f,    1.596f,
        1.164f, -0.391f, -0.813f,
        1.164f,  2.018f,  0.0f
    };
    constexpr float bt601Full[9] = {
        1.0f,  0.0f,    1.402f,
        1.0f, -0.344f, -0.714f,
        1.0f,  1.772f,  0.0f
    };
    constexpr float bt709Limited[9] = {
        1.164f,  0.0f,    1.793f,
        1.164f, -0.213f, -0.533f,
        1.164f,  2.112f,  0.0f
    };
    constexpr float bt709Full[9] = {
        1.0f,  0.0f,    1.574f,
        1.0f, -0.187f, -0.468f,
        1.0f,  1.855f,  0.0f
    };

    // 根据颜色空间和范围选择转换矩阵
    using enum media::Frame::ColorSpace;
    QMatrix3x3 mat3x3;
    mat3x3.setToIdentity();
    if (m_colorSpace == BT601) {
        if (m_colorRange == media::Frame::ColorRange::Limited) {
            mat3x3 = QMatrix3x3(bt601Limited);
        } else {
            mat3x3 = QMatrix3x3(bt601Full);
        }
    } else if (m_colorSpace == BT709) {
        if (m_colorRange == media::Frame::ColorRange::Limited) {
            mat3x3 = QMatrix3x3(bt709Limited);
        } else {
            mat3x3 = QMatrix3x3(bt709Full);
        }
    } else {
        // 默认使用BT601 Limited
        mat3x3 = QMatrix3x3(bt601Limited);
    }

    // 使用 QMatrix4x4，将 Y_OFFSET 存入第四行第一列 [3][0]
    QMatrix4x4 colorTransform(mat3x3);
    colorTransform(3, 0) = yOffset;

    rub->updateDynamicBuffer(m_colorParamsUBuffer.get(), 0, 64, colorTransform.constData());
}
} // namespace view