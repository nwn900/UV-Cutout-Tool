#version 330 core

in  vec2 v_uv;
out vec4 o_color;

uniform sampler2D u_tex;      // diffuse texture (may be default-black)
uniform bool      u_has_tex;
uniform bool      u_alpha_on; // show transparency as a checkerboard
uniform vec2      u_canvas;   // used to stabilize checker size against canvas pixels
uniform vec3      u_bg_color;
uniform vec3      u_checker_dark;
uniform vec3      u_checker_light;

void main() {
    vec4 base = vec4(u_bg_color, 1.0);
    if (u_alpha_on) {
        // 16px checker in screen-space for consistent visual weight at any zoom.
        vec2 f = floor(gl_FragCoord.xy / 16.0);
        bool odd = mod(f.x + f.y, 2.0) > 0.5;
        base = vec4(odd ? u_checker_light : u_checker_dark, 1.0);
    }

    if (u_has_tex) {
        vec4 t = texture(u_tex, v_uv);
        if (u_alpha_on) {
            base.rgb = mix(base.rgb, t.rgb, t.a);
        } else {
            base = vec4(t.rgb, 1.0);
        }
    }
    o_color = base;
}
