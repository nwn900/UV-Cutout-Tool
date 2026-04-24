#version 330 core

// Quad in clip-space (two triangles).
layout(location = 0) in vec2 a_pos;    // [-1, 1]
layout(location = 1) in vec2 a_uv;     // [0, 1]

uniform vec2 u_canvas;      // pixels
uniform vec2 u_uv_origin;   // pixel offset of top-left of the UV 0..1 region on the canvas
uniform vec2 u_uv_size;     // pixel size  of the UV 0..1 region on the canvas

out vec2 v_uv;

void main() {
    // a_pos encodes the quad corner (0,0)..(1,1). Map it through the canvas transform
    // and finally to clip space.
    vec2 pixel_pos = u_uv_origin + a_pos * u_uv_size;
    vec2 ndc = (pixel_pos / u_canvas) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_uv = a_uv;
}
