#version 330 core

in vec2 channelTex;

out vec4 outCol;

uniform sampler2D uTex;

void main()
{
    outCol = texture(uTex, channelTex);
    if (outCol.a < 0.01) discard;
}
