#version 330 core

// UV-space positions in [0, 1]. One vertex per line endpoint.
layout(location = 0) in vec2 a_uv;
layout(location = 1) in vec4 a_color;    // per-vertex color (already resolved for wire/hover/selected)

uniform vec2 u_canvas;      // canvas pixel size
uniform vec2 u_uv_origin;
uniform vec2 u_uv_size;

out vec4 v_color;

void main() {
    vec2 pixel_pos = u_uv_origin + a_uv * u_uv_size;
    vec2 ndc = (pixel_pos / u_canvas) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_color = a_color;
}
