#version 450
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 fragColor;
layout(binding = 0) uniform sampler2D yTexture;
layout(binding = 1) uniform sampler2D uvTexture;
const float Y_OFFSET_LIMITED = 0.0625;
const float UV_OFFSET = 0.5;
const mat3 CONV_709_LIMITED = mat3(
    1.164383,  1.164383,  1.164383,
    0.0,      -0.213249,  2.112402,
    1.792741, -0.532909,  0.0
);
vec3 saturate(vec3 v) { return clamp(v, 0.0, 1.0); }
void main()
{
    float y = texture(yTexture, vTexCoord).r;
    vec2 uv = texture(uvTexture, vTexCoord).rg;
    float Y = y - Y_OFFSET_LIMITED;
    float U = uv.x - UV_OFFSET;
    float V = uv.y - UV_OFFSET;
    vec3 rgb = saturate(CONV_709_LIMITED * vec3(Y, U, V));
    fragColor = vec4(rgb, 1.0);
}