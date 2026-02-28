#version 330 core
layout(location = 0) in vec3 inPos;

uniform mat4 uLightVP;
uniform mat4 uM;

void main()
{
    gl_Position = uLightVP * uM * vec4(inPos, 1.0);
}
