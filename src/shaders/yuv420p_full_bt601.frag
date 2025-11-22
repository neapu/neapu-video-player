#version 450
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 fragColor;
layout(binding = 0) uniform sampler2D yTexture;
layout(binding = 1) uniform sampler2D uTexture;
layout(binding = 2) uniform sampler2D vTexture;
const float UV_OFFSET = 0.5;
const mat3 CONV_601_FULL = mat3(
    1.0,      1.0,       1.0,
    0.0,     -0.344136,  1.772,
    1.402,   -0.714136,  0.0
);
vec3 saturate(vec3 v) { return clamp(v, 0.0, 1.0); }
void main()
{
    float y = texture(yTexture, vTexCoord).r;
    float u = texture(uTexture, vTexCoord).r;
    float v = texture(vTexture, vTexCoord).r;
    float Y = y;
    float U = u - UV_OFFSET;
    float V = v - UV_OFFSET;
    vec3 rgb = saturate(CONV_601_FULL * vec3(Y, U, V));
    fragColor = vec4(rgb, 1.0);
}