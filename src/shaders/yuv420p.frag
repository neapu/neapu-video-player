#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D yTexture;
layout(binding = 1) uniform sampler2D uTexture;
layout(binding = 2) uniform sampler2D vTexture;



void main()
{
    float y = texture(yTexture, vTexCoord).r;
    float u = texture(uTexture, vTexCoord).r;
    float v = texture(vTexture, vTexCoord).r;

    float C = y;
    float D = u - 0.5;
    float E = v - 0.5;

    // BT.601（有限范围）YUV->RGB 转换矩阵（列主序）
    const mat3 kBT601 = mat3(
    1.164383,  1.164383,  1.164383,  // C 列
    0.0,      -0.391762,  2.017232,  // D 列
    1.596027, -0.812968,  0.0        // E 列
    );

    vec3 rgb = kBT601 * vec3(C, D, E);
    rgb = clamp(rgb, 0.0, 1.0);

    fragColor = vec4(rgb, 1.0);
}
