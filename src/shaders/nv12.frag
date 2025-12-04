#version 450
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 fragColor;
layout(binding = 0) uniform sampler2D yTexture;
layout(binding = 1) uniform sampler2D uvTexture;
layout(std140, binding = 4) uniform ColorParams {
    mat4 COLOR_CONVERSION; // mat3 in upper-left, Y_OFFSET in [3][0]
};

const float UV_OFFSET = 0.5;

vec3 saturate(vec3 v) { return clamp(v, 0.0, 1.0); }
void main()
{
    float y = texture(yTexture, vTexCoord).r;
    vec2 uv = texture(uvTexture, vTexCoord).rg;
    float Y = y - COLOR_CONVERSION[3][0]; // Y_OFFSET stored in 4th row
    float U = uv.x - UV_OFFSET;
    float V = uv.y - UV_OFFSET;
    vec3 rgb = saturate(mat3(COLOR_CONVERSION) * vec3(Y, U, V));
    fragColor = vec4(rgb, 1.0);
}