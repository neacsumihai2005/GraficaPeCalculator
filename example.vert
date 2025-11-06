#version 330 core

layout(location = 0) in vec4 a_position;
layout(location = 1) in float a_life; // used for particles (ignored for car/line)

uniform mat4 u_projection;
uniform vec2 u_offset;   // per-object offset (world-space)
uniform vec2 u_camera;   // camera offset (subtract player position)
uniform float u_rotation; // radians

out float v_life;

void main()
{
    // For both cars and particles:
    vec2 pos = vec2(a_position.x, a_position.y);

    // apply object-local rotation (for car/line)
    float c = cos(u_rotation);
    float s = sin(u_rotation);
    vec2 rotated = vec2(c * pos.x - s * pos.y, s * pos.x + c * pos.y);

    // world position (with offset)
    vec2 worldPos = rotated + u_offset;

    // camera: shift by camera (camera contains -playerPos so player stays centered)
    vec2 finalPos = worldPos + u_camera;

    gl_Position = u_projection * vec4(finalPos, 0.0, 1.0);

    // point size for particles: if input vertex has small life encoded in a_life, use it
    v_life = a_life;
    // set a default point size (will be used by particle rendering path)
    gl_PointSize = 8.0;
}
