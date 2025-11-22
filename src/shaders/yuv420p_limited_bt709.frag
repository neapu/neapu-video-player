#version 450
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 fragColor;
layout(binding = 0) uniform sampler2D yTexture;
layout(binding = 1) uniform sampler2D uTexture;
layout(binding = 2) uniform sampler2D vTexture;
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
    float u = texture(uTexture, vTexCoord).r;
    float v = texture(vTexture, vTexCoord).r;
    float Y = y - Y_OFFSET_LIMITED;
    float U = u - UV_OFFSET;
    float V = v - UV_OFFSET;
    vec3 rgb = saturate(CONV_709_LIMITED * vec3(Y, U, V));
    fragColor = vec4(rgb, 1.0);
}