//
// Created by liu86 on 2025/11/15.
//

#pragma once
#include <QRhiWidget>
#include <rhi/qrhi.h>
#include <mutex>

#include "../media/Frame.h"
#include "Pipeline.h"

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
    void releaseResources() override;

    void start(double fps, int64_t startTimeUs);
    void stop();
    bool isRunning() const { return m_running.load(); }

#ifdef _WIN32
    ID3D11Device* getD3D11Device();
#endif

signals:
    void initialized();

private:
    void RenderFrame(QRhiCommandBuffer* cb);
    void RenderEmpty(QRhiCommandBuffer* cb);
    void RenderImpl(QRhiResourceUpdateBatch* rub, QRhiCommandBuffer* cb);

private:
    QRhi* m_rhi{nullptr};
    std::unique_ptr<QRhiBuffer> m_vertexBuffer{};
    std::unique_ptr<Pipeline> m_pipeline{};

    std::atomic_bool m_running{false};

#ifdef _WIN32
    ID3D11Device* m_d3d11Device{nullptr};
    ID3D11DeviceContext* m_d3d11DeviceContext{nullptr};
#endif
};

} // namespace view
