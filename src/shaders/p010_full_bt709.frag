#version 450
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 fragColor;
layout(binding = 0) uniform sampler2D yTexture;
layout(binding = 1) uniform sampler2D uvTexture;
const float UV_OFFSET = 0.5;
const mat3 CONV_709_FULL = mat3(
    1.0,      1.0,       1.0,
    0.0,     -0.187300,  1.855600,
    1.574800, -0.468100, 0.0
);
vec3 saturate(vec3 v) { return clamp(v, 0.0, 1.0); }
void main()
{
    float y = texture(yTexture, vTexCoord).r;
    vec2 uv = texture(uvTexture, vTexCoord).rg;
    float Y = y;
    float U = uv.x - UV_OFFSET;
    float V = uv.y - UV_OFFSET;
    vec3 rgb = saturate(CONV_709_FULL * vec3(Y, U, V));
    fragColor = vec4(rgb, 1.0);
}