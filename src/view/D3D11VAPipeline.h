//
// Created by liu86 on 2025/12/4.
//

#pragma once
#include "Pipeline.h"
#ifdef _WIN32
#include <d3d11.h>
#include <wrl/client.h>

namespace view {

class D3D11VAPipeline : public Pipeline {
public:
    D3D11VAPipeline(QRhi* rhi, ID3D11Device* d3d11Device, ID3D11DeviceContext* d3d11DeviceContext);
    ~D3D11VAPipeline() override;

    bool create(const media::FramePtr& frame, QRhiRenderTarget* renderTarget) override;

    void updateTexture(QRhiResourceUpdateBatch* rub, media::FramePtr&& frame) override;
protected:
    bool createSrb() override;
    QString getFragmentShaderName() override;

protected:
    std::unique_ptr<QRhiTexture> m_yTexture{};
    std::unique_ptr<QRhiTexture> m_uvTexture{};

    ID3D11Device* m_d3d11Device{ nullptr };
    ID3D11DeviceContext* m_d3d11DeviceContext{ nullptr };
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_nativeTexture{ nullptr };
    DXGI_FORMAT m_textureFormat{ DXGI_FORMAT_UNKNOWN };
};
} // namespace view
#endif