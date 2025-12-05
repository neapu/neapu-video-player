//
// Created by liu86 on 2025/12/4.
//

#pragma once
#include "Pipeline.h"

namespace view {

class YuvPipeline : public Pipeline {
public:
    explicit YuvPipeline(QRhi* rhi);
    ~YuvPipeline() override;

    void updateTexture(QRhiResourceUpdateBatch* rub, media::FramePtr&& frame) override;

protected:
    bool createSrb(const QSize& size) override;
    QString getFragmentShaderName() override;

protected:

    std::unique_ptr<QRhiTexture> m_yTexture{};
    std::unique_ptr<QRhiTexture> m_uTexture{};
    std::unique_ptr<QRhiTexture> m_vTexture{};
};

} // namespace view
