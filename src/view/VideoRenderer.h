//
// Created by liu86 on 2025/11/15.
//

#pragma once
#include <QRhiWidget>
#include <rhi/qrhi.h>
#include <mutex>

#include "../media/Frame.h"

#ifdef _WIN32
#include <d3d11.h>
#include <wrl/client.h>
#endif

namespace view {

class VideoRenderer:public QRhiWidget {
    Q_OBJECT
public:
    explicit VideoRenderer(QWidget* parent = nullptr);
    ~VideoRenderer() override;

    void initialize(QRhiCommandBuffer* cb) override;
    void render(QRhiCommandBuffer* cb) override;

    void start(double fps);
    void stop();

    void seek(int serial);

    // void onFrameReady(media::FramePtr&& newFrame);

#ifdef _WIN32
    ID3D11Device* getD3D11Device();
#endif

public slots:
    void onAudioPtsUpdated(int64_t ptsUs);

signals:
    void initialized();
    void eof();
    void playingPts(int64_t ptsUs);

private:
    bool recreatePipeline();
    bool createEmptyPipeline();
    bool createPipeline();


    bool recreateTextures();
    bool createYUVTextures();
    bool createD3D11Texture();

    void updateTextures(QRhiResourceUpdateBatch* rub);
    void updateYUVTextures(QRhiResourceUpdateBatch* rub);
    void updateD3D11Texture();

    void updateVertexUniformBuffer(QRhiResourceUpdateBatch* rub, QSize renderSize);

    QString getFragmentShaderName();

private:
    QRhi* m_rhi{nullptr};
    std::unique_ptr<QRhiGraphicsPipeline> m_pipeline{};
    std::unique_ptr<QRhiBuffer> m_vertexBuffer{};
    std::unique_ptr<QRhiShaderResourceBindings> m_srb{};
    std::unique_ptr<QRhiSampler> m_sampler{};
    std::unique_ptr<QRhiTexture> m_textures[3]{};
    std::unique_ptr<QRhiBuffer> m_vsUBuffer{}; // 用于顶点着色器的uniform缓冲区，用于传递缩放矩阵

    media::FramePtr m_currentFrame{};
    media::FramePtr m_nextFrame{};

    QSize m_lastVideoSize{0, 0};
    QSize m_lastRenderSize{0, 0};

    std::atomic_bool m_running{false};
    std::atomic<double> m_fps{30.0};
    std::atomic<int64_t> m_startTimeUs{0};
    std::atomic_int m_serial{0};

#ifdef _WIN32
    ID3D11Device* m_d3d11Device{nullptr};
    ID3D11DeviceContext* m_d3d11DeviceContext{nullptr};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_nativeTexture{nullptr};
#endif
};

} // namespace view
