#version 330 core

in vec4 channelCol;
in vec2 channelTex;
in vec3 fragPos;
in vec3 fragNormal;

out vec4 outCol;

uniform sampler2D uTex;
uniform bool useTex;
uniform bool transparent;

uniform vec3 uViewPos;
uniform float uShine;

/* Glavni izvor svetla gore (veliki) – Fongov model: kA, kD, kS */
uniform vec3 uLightPos;
uniform vec3 uLightKA;
uniform vec3 uLightKD;
uniform vec3 uLightKS;

/* Slabi izvor – ekran sata */
uniform vec3 uLight2Pos;
uniform vec3 uLight2KA;
uniform vec3 uLight2KD;
uniform vec3 uLight2KS;
uniform bool uLight2On;

uniform bool uUnlit;

/* Senka od glavnog svetla (zgrade/ruka bacaju senku na put) */
uniform sampler2D uShadowMap;
uniform mat4 uLightSpaceMatrix;

void main()
{
    if (uUnlit) {
        outCol = texture(uTex, channelTex);
        if (outCol.a < 0.01) discard;
        return;
    }
    vec4 baseCol = useTex ? texture(uTex, channelTex) : channelCol;
    if (!transparent && useTex && baseCol.a < 1.0)
        baseCol = vec4(1.0, 1.0, 1.0, 1.0);

    vec3 mat = baseCol.rgb;

    vec3 norm = normalize(fragNormal);
    vec3 viewDir = normalize(uViewPos - fragPos);

    /* Da li je fragment u senci (zbog zgrada, ruke, itd.) od glavnog svetla? */
    vec4 fragPosLight = uLightSpaceMatrix * vec4(fragPos, 1.0);
    vec3 lightNDC = fragPosLight.xyz / fragPosLight.w;
    vec2 shadowTexCoord = lightNDC.xy * 0.5 + 0.5;
    float depthFrag = lightNDC.z * 0.5 + 0.5;
    float bias = 0.004;
    float depthFromMap = texture(uShadowMap, shadowTexCoord).r;
    float inShadow = 0.0;
    if (shadowTexCoord.x >= 0.0 && shadowTexCoord.x <= 1.0 && shadowTexCoord.y >= 0.0 && shadowTexCoord.y <= 1.0 && depthFrag <= 1.0)
        inShadow = (depthFrag > depthFromMap + bias) ? 1.0 : 0.0;

    /* Glavno svetlo gore – ambijent + difuzno + spekularno; u senci slabije */
    vec3 lightDir = normalize(uLightPos - fragPos);
    float NdotL = max(dot(norm, lightDir), 0.0);
    vec3 resA = uLightKA * mat;
    vec3 resD = uLightKD * (NdotL * mat);
    vec3 reflDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflDir), 0.0), uShine);
    vec3 resS = uLightKS * (spec * mat);
    float shadowFactor = 1.0 - inShadow * 0.75;
    vec3 result = resA + (resD + resS) * shadowFactor;

    /* Slabi izvor – ekran sata; slabi sa udaljenošću da se vidi samo na ruci / kućištu */
    if (uLight2On) {
        vec3 toLight2 = uLight2Pos - fragPos;
        float dist2 = dot(toLight2, toLight2);
        float att = 1.0 / (1.0 + 0.4 * dist2);  /* blizu = jače, daleko = slabije */
        vec3 lightDir2 = normalize(toLight2);
        float NdotL2 = max(dot(norm, lightDir2), 0.0);
        result += att * (uLight2KA * mat);
        result += att * (uLight2KD * (NdotL2 * mat));
        vec3 reflDir2 = reflect(-lightDir2, norm);
        float spec2 = pow(max(dot(viewDir, reflDir2), 0.0), uShine);
        result += att * (uLight2KS * (spec2 * mat));
    }

    outCol = vec4(result, baseCol.a);
}
