#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 fragColor;

// 没有视频帧输出时，渲染纯色
void main()
{
    fragColor = vec4(0.0, 0.0, 0.0, 1.0);
}