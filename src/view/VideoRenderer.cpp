//
// Created by liu86 on 2025/11/15.
//

#include "VideoRenderer.h"

namespace view {
VideoRenderer::VideoRenderer(QWidget* parent)
    : QRhiWidget(parent)
{}
void VideoRenderer::initialize(QRhiCommandBuffer* cb)
{
    // TODO: 实现视频初始化逻辑
}
void VideoRenderer::render(QRhiCommandBuffer* cb)
{
    // TODO: 实现视频渲染逻辑
}

} // namespace view