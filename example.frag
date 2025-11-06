#version 330 core

in float v_life;
out vec4 FragColor;
uniform vec3 u_color;

void main()
{
    // If this fragment is part of a point (particle), v_life will be > 0 for particles,
    // but for cars/lines the attribute a_life is zero; however we don't rely on that heavily.
    // We'll draw everything with u_color; for particles we can modulate alpha by v_life if needed.
    float alpha = 1.0;
    if (v_life > 0.0) {
        alpha = clamp(v_life, 0.0, 1.0);
        // soft circular particle (point sprite) fade
        vec2 coord = gl_PointCoord - vec2(0.5);
        float dist = length(coord);
        alpha *= smoothstep(0.6, 0.0, dist);
    }
    FragColor = vec4(u_color, alpha);
}
