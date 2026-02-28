#version 330 core
layout(location = 0) in vec2 inPos;
out vec2 chTex;
void main()
{
    gl_Position = vec4(inPos, 0.0, 1.0);
    chTex = vec2(0.0, 0.0);
}
