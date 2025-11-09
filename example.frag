#version 330 core
in vec2 vUV;
flat in int vMode;
uniform sampler2D uTex;
uniform vec4 uColor;
out vec4 fragColor;
void main(){
    if(vMode == 0){
        fragColor = texture(uTex, vUV);
    } else {
        fragColor = uColor;
    }
}
