#version 330 core

in vec2 channelTex;
in vec2 vPos;

out vec4 outCol;

uniform sampler2D uTex;
uniform float uAlpha;
uniform vec4 uColor;
uniform bool uUseColor;
/* Za okvir sata: elipsa (ne pravougaonik). Ako uEllipseRx > 0, odbacujemo fragmente van elipse. */
uniform float uEllipseRx;
uniform float uEllipseRy;

void main()
{
    if (uEllipseRx > 0.0 && uEllipseRy > 0.0) {
        float d = (vPos.x * vPos.x) / (uEllipseRx * uEllipseRx) + (vPos.y * vPos.y) / (uEllipseRy * uEllipseRy);
        if (d > 1.0) discard;
    }
    if (uUseColor)
        outCol = uColor;
    else {
        outCol = texture(uTex, channelTex);
        outCol.a *= uAlpha;
    }
}
