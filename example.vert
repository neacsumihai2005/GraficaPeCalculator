#version 330 core
layout(location=0) in vec2 inPos;
layout(location=1) in vec2 inUV;
uniform mat4 codColVert;
uniform mat4 uProj;
uniform int codColShader;
out vec2 vUV;
flat out int vMode;
void main(){
    vec4 world = codColVert * vec4(inPos.xy, 0.0, 1.0);
    gl_Position = uProj * world;
    vUV = inUV;
    vMode = codColShader;
}
