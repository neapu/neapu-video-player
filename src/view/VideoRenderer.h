//
// Created by liu86 on 2025/10/28.
//

#pragma once
#include <QRhiWidget>
#include <thread>
#include "../media/VideoFrame.h"
#include <functional>
#include <atomic>
#include <mutex>
#include <rhi/qrhi.h>
#ifdef _WIN32
#include <d3d11.h>
#endif

namespace view {
using VideoFrameCallback = std::function<media::VideoFramePtr()>;
class VideoRenderer final : public QRhiWidget {
    Q_OBJECT
public:
    explicit VideoRenderer(const VideoFrameCallback& cb, QWidget* parent = nullptr);
    ~VideoRenderer() override;

    void start();
    void stop(bool flush);

    void initialize(QRhiCommandBuffer* cb) override;
    void render(QRhiCommandBuffer* cb) override;

#ifdef _WIN32
    ID3D11Device* d3d11Device() const;
    ID3D11DeviceContext* d3d11DeviceContext() const;
#endif

signals:
    void initialized();

private:
    void recvFrameThread();
    bool createPipeline();
    bool createEmptyPipeline();
    QString getFragmentShaderName();
    bool updateTexture(QRhiCommandBuffer* cb, QRhiResourceUpdateBatch* rub);
    void updateVertexUniforms(QRhiCommandBuffer* cb, QRhiResourceUpdateBatch* rub);
    void updateFragmentUniforms(QRhiCommandBuffer* cb, QRhiResourceUpdateBatch* rub);
    bool createTextures();
    bool createYUVTextures();
    bool createD3D11Texture();
    bool updateYUVTextures(QRhiCommandBuffer* cb, QRhiResourceUpdateBatch* rub);
    bool updateD3D11Texture(QRhiCommandBuffer* cb, QRhiResourceUpdateBatch* rub);

private:
    VideoFrameCallback m_videoFrameCallback;
    std::thread m_recvFrameThread;
    std::atomic_bool m_stopFlag{true};

    media::VideoFramePtr m_currentFrame{nullptr};
    std::mutex m_frameMutex;

    QRhi* m_rhi{nullptr};
    std::unique_ptr<QRhiGraphicsPipeline> m_pipeline{nullptr};
    std::unique_ptr<QRhiBuffer> m_vertexBuffer{nullptr};
    std::unique_ptr<QRhiShaderResourceBindings> m_srb{nullptr};
    std::unique_ptr<QRhiSampler> m_sampler{nullptr};
    std::unique_ptr<QRhiTexture> m_yTexture{nullptr};
    std::unique_ptr<QRhiTexture> m_uTexture{nullptr};
    std::unique_ptr<QRhiTexture> m_vTexture{nullptr};
    std::unique_ptr<QRhiTexture>& m_uvTexture{m_uTexture}; // for NV12/P010
    std::unique_ptr<QRhiBuffer> m_vsUBuffer{nullptr}; // 用于顶点着色器的uniform缓冲区
    std::unique_ptr<QRhiBuffer> m_fsUBuffer{nullptr}; // 用于片段着色器的uniform缓冲区

    int m_currentWidth{0};
    int m_currentHeight{0};
    media::VideoFrame::PixelFormat m_currentPixelFormat{media::VideoFrame::PixelFormat::NONE};
    bool m_videoSizeChanged{false}; // 用于判断是否需要重新计算缩放矩阵
    QSize m_lastRenderSize{0, 0};

    int m_colorSpace{0};    // 0:bt601/1:bt709/2:bt2020
    int m_colorRange{0};    // 0:limited/1:full
    int m_colorTransfer{0}; // 0:bt709/1:hdr10/2:hlog

#ifdef _WIN32
    ID3D11Device* m_d3d11Device{nullptr};
    ID3D11DeviceContext* m_d3d11DeviceContext{nullptr};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_d3d11Texture{nullptr};
#endif
};

} // namespace view
