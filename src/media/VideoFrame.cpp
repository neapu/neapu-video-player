//
// Created by liu86 on 2025/10/30.
//

#include "VideoFrame.h"

#include "logger.h"
extern "C" {
#include <libavutil/frame.h>
#include <libavutil/avutil.h>
}

namespace media {
VideoFrame::~VideoFrame()
{
    if (m_yData) {
        delete[] m_yData;
        m_yData = nullptr;
    }
    if (m_uData) {
        delete[] m_uData;
        m_uData = nullptr;
    }
    if (m_vData) {
        delete[] m_vData;
        m_vData = nullptr;
    }
}
std::unique_ptr<VideoFrame> VideoFrame::fromAVFrame(const void* frame, AVRational timeBase)
{
    if (!frame) {
        return nullptr;
    }

    auto ret = std::make_unique<VideoFrame>();
    auto* avFrame = static_cast<const AVFrame*>(frame);
    if (avFrame->format == AV_PIX_FMT_YUV420P) {
        ret->m_pixelFormat = PixelFormat::YUV420P;
        ret->m_ySize = avFrame->width * avFrame->height;
        ret->m_uSize = (avFrame->width / 2) * (avFrame->height / 2);
        ret->m_vSize = (avFrame->width / 2) * (avFrame->height / 2);
        ret->m_yData = new uint8_t[ret->m_ySize];
        ret->m_uData = new uint8_t[ret->m_uSize];
        ret->m_vData = new uint8_t[ret->m_vSize];
        // 拷贝Y平面
        for (int i = 0; i < avFrame->height; ++i) {
            std::memcpy(ret->m_yData + i * avFrame->width, avFrame->data[0] + i * avFrame->linesize[0], avFrame->width);
        }
        // 拷贝U平面
        for (int i = 0; i < avFrame->height / 2; ++i) {
            std::memcpy(ret->m_uData + i * (avFrame->width / 2), avFrame->data[1] + i * avFrame->linesize[1], avFrame->width / 2);
        }
        // 拷贝V平面
        for (int i = 0; i < avFrame->height / 2; ++i) {
            std::memcpy(ret->m_vData + i * (avFrame->width / 2), avFrame->data[2] + i * avFrame->linesize[2], avFrame->width / 2);
        }
    } else if (avFrame->format == AV_PIX_FMT_D3D11) {
        ret->m_pixelFormat = PixelFormat::D3D11;
#ifdef _WIN32
        auto* texture = reinterpret_cast<ID3D11Texture2D*>(avFrame->data[0]);
        int subresource = static_cast<int>(reinterpret_cast<uintptr_t>(avFrame->data[1]));

        using Microsoft::WRL::ComPtr;
        ComPtr<ID3D11Device> device;
        texture->GetDevice(&device);
        ComPtr<ID3D11DeviceContext> context;
        device->GetImmediateContext(&context);

        D3D11_TEXTURE2D_DESC desc;
        texture->GetDesc(&desc);
        if (desc.Format != DXGI_FORMAT_NV12 && desc.Format != DXGI_FORMAT_P010) {
            NEAPU_LOGE("Unsupported D3D11 texture format: {}", static_cast<int>(desc.Format));
            return nullptr;
        }

        D3D11_TEXTURE2D_DESC texDesc{};
        texDesc.Width = avFrame->width;
        texDesc.Height = avFrame->height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = desc.Format;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = 0;
        texDesc.MiscFlags = 0;

        auto hr = device->CreateTexture2D(&texDesc, nullptr, &ret->d3d11Texture);
        if (FAILED(hr)) {
            NEAPU_LOGE("Failed to create D3D11 texture: HRESULT={}", hr);
            return nullptr;
        }

        D3D11_BOX srcBox{};
        srcBox.left = 0;
        srcBox.top = 0;
        srcBox.front = 0;
        srcBox.right = avFrame->width;
        srcBox.bottom = avFrame->height;
        srcBox.back = 1;

        context->CopySubresourceRegion(ret->d3d11Texture.Get(), 0, 0, 0, 0, texture, subresource, &srcBox);
#endif
    } else {
        NEAPU_LOGE("Unsupported pixel format: {}", static_cast<int>(avFrame->format));
        return nullptr;
    }

    ret->m_width = avFrame->width;
    ret->m_height = avFrame->height;
    if (avFrame->duration > 0 && avFrame->time_base.num > 0 && avFrame->time_base.den > 0) {
        ret->m_durationUs = av_rescale_q(avFrame->duration, avFrame->time_base, AV_TIME_BASE_Q);
    } else if (avFrame->duration > 0 && timeBase.den > 0) {
        ret->m_durationUs = av_rescale_q(avFrame->duration, timeBase, AV_TIME_BASE_Q);
    } else {
        ret->m_durationUs = 0;
    }
    if (avFrame->pts != AV_NOPTS_VALUE && avFrame->time_base.den != 0 && avFrame->time_base.num != 0) {
        ret->m_pts = av_rescale_q(avFrame->pts, avFrame->time_base, AV_TIME_BASE_Q);
    } else if (avFrame->pts != AV_NOPTS_VALUE && timeBase.den != 0) {
        ret->m_pts = av_rescale_q(avFrame->pts, timeBase, AV_TIME_BASE_Q);
    } else {
        ret->m_pts = -1;
    }

    switch (avFrame->colorspace) {
    case AVCOL_SPC_BT709:
        ret->m_colorSpace = ColorSpace::BT709;
        break;
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:
        ret->m_colorSpace = ColorSpace::BT2020;
        break;
    default:
        ret->m_colorSpace = ColorSpace::BT601;
    }

    switch (avFrame->color_range) {
    case AVCOL_RANGE_JPEG:
        ret->m_colorRange = ColorRange::FULL;
        break;
    default:
        ret->m_colorRange = ColorRange::LIMITED;
    }

    switch (avFrame->color_trc) {
    case AVCOL_TRC_SMPTE2084:
        ret->m_colorTransfer = ColorTransfer::HDR10;
        break;
    case AVCOL_TRC_ARIB_STD_B67:
        ret->m_colorTransfer = ColorTransfer::HLOG;
        break;
    default:
        ret->m_colorTransfer = ColorTransfer::BT709;
    }

    return ret;
}
} // namespace media