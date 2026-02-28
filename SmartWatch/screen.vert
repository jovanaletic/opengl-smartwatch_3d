#version 330 core

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inTex;

out vec2 channelTex;
out vec2 vPos;

uniform mat4 uP;

void main()
{
    gl_Position = uP * vec4(inPos, 0.0, 1.0);
    channelTex = inTex;
    vPos = inPos;
}
