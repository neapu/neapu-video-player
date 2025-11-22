//
// Created by liu86 on 2025/11/18.
//

#include "Frame.h"
#include <algorithm>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
}

#ifdef _WIN32
#include <d3d11.h>
#endif

namespace media {

namespace {
static inline AVFrame* toAv(void* p) { return reinterpret_cast<AVFrame*>(p); }
}

Frame::Frame(void* avFrame)
    : m_avFrame(avFrame)
{
}

Frame::~Frame()
{
    if (m_avFrame) {
        AVFrame* f = toAv(m_avFrame);
        av_frame_free(&f);
        m_avFrame = nullptr;
    }
}

Frame::Frame(Frame&& other) noexcept
    : m_avFrame(other.m_avFrame)
{
    other.m_avFrame = nullptr;
}

Frame& Frame::operator=(Frame&& other) noexcept
{
    if (this != &other) {
        if (m_avFrame) {
            AVFrame* f = toAv(m_avFrame);
            av_frame_free(&f);
        }
        m_avFrame = other.m_avFrame;
        other.m_avFrame = nullptr;
    }
    return *this;
}

const uint8_t* Frame::data(int index) const
{
    AVFrame* f = toAv(m_avFrame);
    if (!f || index < 0 || index >= AV_NUM_DATA_POINTERS) return nullptr;
    return f->data[index];
}

int Frame::lineSize(int index) const
{
    AVFrame* f = toAv(m_avFrame);
    if (!f || index < 0 || index >= AV_NUM_DATA_POINTERS) return 0;
    return f->linesize[index];
}

int Frame::width() const
{
    AVFrame* f = toAv(m_avFrame);
    return f ? f->width : 0;
}

int Frame::height() const
{
    AVFrame* f = toAv(m_avFrame);
    return f ? f->height : 0;
}

int64_t Frame::ptsUs() const
{
    AVFrame* f = toAv(m_avFrame);
    if (!f) return 0;
    int64_t ts = 0;
    if (f->pts != AV_NOPTS_VALUE) ts = f->pts;
    else if (f->best_effort_timestamp != AV_NOPTS_VALUE) ts = f->best_effort_timestamp;
    else return 0;
    AVRational tb = f->time_base;
    if (tb.num > 0 && tb.den > 0) {
        return av_rescale_q(ts, tb, AVRational{1, 1000000});
    }
    return ts;
}

int64_t Frame::durationUs() const
{
    AVFrame* f = toAv(m_avFrame);
    if (!f) return 0;
    int64_t dur = f->duration;
    if (dur <= 0) return 0;
    AVRational tb = f->time_base;
    if (tb.num > 0 && tb.den > 0) {
        return av_rescale_q(dur, tb, AVRational{1, 1000000});
    }
    return dur;
}

Frame::PixelFormat Frame::pixelFormat() const
{
    AVFrame* f = toAv(m_avFrame);
    if (!f) return PixelFormat::UNKNOWN;
    switch (static_cast<AVPixelFormat>(f->format)) {
        case AV_PIX_FMT_YUV420P: return PixelFormat::YUV420P;
#ifdef _WIN32
        case AV_PIX_FMT_D3D11: return PixelFormat::D3D11Texture2D;
#endif
        default: return PixelFormat::UNKNOWN;
    }
}

Frame::ColorSpace Frame::colorSpace() const
{
    AVFrame* f = toAv(m_avFrame);
    if (!f) return ColorSpace::BT601;
    switch (f->colorspace) {
        case AVCOL_SPC_BT709: return ColorSpace::BT709;
        default: return ColorSpace::BT601;
    }
}

Frame::ColorRange Frame::colorRange() const
{
    AVFrame* f = toAv(m_avFrame);
    if (!f) return ColorRange::Limited;
    switch (f->color_range) {
        case AVCOL_RANGE_JPEG: return ColorRange::Full;
        default: return ColorRange::Limited;
    }
}

uint8_t* Frame::yData() const
{
    AVFrame* f = toAv(m_avFrame);
    return f ? f->data[0] : nullptr;
}

uint8_t* Frame::uData() const
{
    AVFrame* f = toAv(m_avFrame);
    return f ? f->data[1] : nullptr;
}

uint8_t* Frame::vData() const
{
    AVFrame* f = toAv(m_avFrame);
    return f ? f->data[2] : nullptr;
}

int Frame::yLineSize() const
{
    return lineSize(0);
}

int Frame::uLineSize() const
{
    return lineSize(1);
}

int Frame::vLineSize() const
{
    return lineSize(2);
}

#ifdef _WIN32
ID3D11Texture2D* Frame::d3d11Texture2D() const
{
    AVFrame* f = toAv(m_avFrame);
    if (!f) return nullptr;
    if (static_cast<AVPixelFormat>(f->format) != AV_PIX_FMT_D3D11) return nullptr;
    return reinterpret_cast<ID3D11Texture2D*>(f->data[0]);
}

int Frame::subresourceIndex() const
{
    AVFrame* f = toAv(m_avFrame);
    if (!f) return 0;
    if (static_cast<AVPixelFormat>(f->format) != AV_PIX_FMT_D3D11) return 0;
    return static_cast<int>(reinterpret_cast<uintptr_t>(f->data[1]));
}
#endif

int Frame::sampleRate() const
{
    AVFrame* f = toAv(m_avFrame);
    return f ? f->sample_rate : 0;
}

int Frame::channels() const
{
    AVFrame* f = toAv(m_avFrame);
    if (!f) return 0;
#if LIBAVUTIL_VERSION_MAJOR >= 57
    return f->ch_layout.nb_channels;
#else
    return f->channels;
#endif
}

int64_t Frame::nbSamples() const
{
    AVFrame* f = toAv(m_avFrame);
    return f ? f->nb_samples : 0;
}
} // namespace media