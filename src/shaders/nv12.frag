#version 450

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D yTexture;
layout(binding = 1) uniform sampler2D uvTexture; // NV12: 交织 UV，取 .rg

layout(std140, binding = 4) uniform ColorParams {
    ivec4 u_color; // x:space, y:range, z:transfer(预留)
};

// ---- 常量与工具函数 ----
const int CS_BT601  = 0;
const int CS_BT709  = 1;
const int CS_BT2020 = 2;

const int RANGE_LIMITED = 0;
const int RANGE_FULL    = 1;

const float Y_OFFSET_LIMITED = 0.0625; // 16/255
const float UV_OFFSET        = 0.5;    // 中心偏移

// LIMITED 范围的转换矩阵（列主序）
const mat3 CONV_601_LIMITED = mat3(
    1.164383,  1.164383,  1.164383,
    0.0,      -0.391762,  2.017232,
    1.596027, -0.812968,  0.0
);
const mat3 CONV_709_LIMITED = mat3(
    1.164383,  1.164383,  1.164383,
    0.0,      -0.213249,  2.112402,
    1.792741, -0.532909,  0.0
);
const mat3 CONV_2020_LIMITED = mat3(
    1.164383,  1.164383,  1.164383,
    0.0,      -0.187520,  2.141770,
    1.678670, -0.650430,  0.0
);

// FULL 范围的转换矩阵（列主序）
const mat3 CONV_601_FULL = mat3(
    1.0,      1.0,       1.0,
    0.0,     -0.344136,  1.772,
    1.402,   -0.714136,  0.0
);
const mat3 CONV_709_FULL = mat3(
    1.0,      1.0,       1.0,
    0.0,     -0.187300,  1.855600,
    1.574800, -0.468100, 0.0
);
const mat3 CONV_2020_FULL = mat3(
    1.0,      1.0,       1.0,
    0.0,     -0.164600,  1.881400,
    1.474600, -0.571400, 0.0
);

mat3 selectConvMatrix(int cs, int range)
{
    if (range == RANGE_LIMITED) {
        if (cs == CS_BT601)  return CONV_601_LIMITED;
        if (cs == CS_BT709)  return CONV_709_LIMITED;
        return CONV_2020_LIMITED;
    } else {
        if (cs == CS_BT601)  return CONV_601_FULL;
        if (cs == CS_BT709)  return CONV_709_FULL;
        return CONV_2020_FULL;
    }
}

vec3 saturate(vec3 v) { return clamp(v, 0.0, 1.0); }

void main()
{
    float y = texture(yTexture, vTexCoord).r;
    vec2 uv = texture(uvTexture, vTexCoord).rg; // NV12: U=R, V=G

    int cs = u_color.x;
    int range = u_color.y;

    float Y = y;
    float U = uv.x - UV_OFFSET;
    float V = uv.y - UV_OFFSET;
    if (range == RANGE_LIMITED) {
        Y -= Y_OFFSET_LIMITED;
    }

    mat3 conv = selectConvMatrix(cs, range);
    vec3 rgb = saturate(conv * vec3(Y, U, V));
    fragColor = vec4(rgb, 1.0);
}