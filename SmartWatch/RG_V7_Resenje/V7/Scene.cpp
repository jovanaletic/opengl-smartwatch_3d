#define _CRT_SECURE_NO_WARNINGS
/*
 * Scene – 3D scena: kamera, pod, zgrade, ruka, sat, pozadina, senke.
 * Sadrzi inicijalizaciju (šaderi, VAO, teksture, shadow map) i render (shadow pass,
 * fullscreen pozadina, glavni prolaz sa osvetljenjem i crtanjem objekata).
 */
#include "Scene.h"
#include "Util.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>

static const float GROUND_SEGMENT_LENGTH = 25.0f;
static const int GROUND_SEGMENTS = 20;
/* Reset tek kad je segment iza kamere (z>5), da ne bi bilo procepa ispod igrača (z≈0). */
static const float GROUND_RESET_Z = 30.0f;
static const float GROUND_CYCLE_LENGTH = GROUND_SEGMENTS * GROUND_SEGMENT_LENGTH;

static const float watchWorldX = 2.0f;
static const float watchWorldZ = 0.0f;
static const float WATCH_OFFSET_ON_HAND = 0.12f;
static const float WATCH_OFFSET_X = 0.02f;
static const float WATCH_OFFSET_Y = 0.04f;
static const float WATCH_OFFSET_Z_RAISED = 0.12f;
static const float WATCH_OFFSET_Z_NORMAL = 0.02f;
static const float RAISED_ARM_DIST = 1.35f;

static float cameraPitch = 0.0f;
static float cameraYaw = -90.0f;
static glm::vec3 cameraPos = glm::vec3(0.0f, 1.6f, 5.0f);
static glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
static glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
static bool firstMouse = true;
static float lastMouseX = 0.0f, lastMouseY = 0.0f;
static bool cameraLocked = false;

static unsigned int shaderMain = 0, shaderScreen = 0, shaderOverlay = 0, shaderDepth = 0;
static unsigned int VAO_cube = 0, VAO_quad = 0, VAO_ground = 0, VAO_overlay = 0, VAO_screen3D = 0, VAO_hand = 0;
static unsigned int VAO_fullscreen = 0;
static unsigned int VBO_quad = 0;
static int handVertexCount = 0;
static unsigned int texGround = 0, texBuilding = 0, texStudent = 0, texSkin = 0, texWatchCase = 0, texSky = 0;
static unsigned int shadowFBO = 0, shadowMapTex = 0;
static const int SHADOW_MAP_SIZE = 2048;

/* Učitava sliku u teksturu i podešava repeat + mipmap (za pod, zidove). */
static unsigned int preprocessTexture(const char* filepath) {
    unsigned int t = loadImageToTexture(filepath);
    if (t == 0) return 0;
    glBindTexture(GL_TEXTURE_2D, t);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return t;
}

/* Ako nema grass.png */
static unsigned int createGrassTexture() {
    const int S = 128;
    unsigned char* img = (unsigned char*)malloc(S * S * 4);
    if (!img) return 0;
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            int i = (y * S + x) * 4;
            int n = (x * 7 + y * 13 + (x * y) % 17) % 31;
            unsigned char g = (unsigned char)(60 + n);
            unsigned char r = (unsigned char)(25 + n / 2);
            unsigned char b = (unsigned char)(30 + n / 3);
            img[i] = r; img[i+1] = g; img[i+2] = b; img[i+3] = 255;
        }
    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, S, S, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    free(img);
    return tex;
}

/* Siva metalna tekstura za kućište sata (proceduralno). */
static unsigned int createWatchCaseTexture() {
    const int S = 64;
    unsigned char* img = (unsigned char*)malloc(S * S * 4);
    if (!img) return 0;
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            int i = (y * S + x) * 4;
            unsigned char g = (unsigned char)(45 + (x ^ y) % 25);
            img[i] = g; img[i+1] = g; img[i+2] = g + 10; img[i+3] = 255;
        }
    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, S, S, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    free(img);
    return tex;
}

/* Sivkasta „cigla” za zgrade ako nema building.png/jpeg */
static unsigned int createBuildingTexture() {
    const int S = 64;
    unsigned char* img = (unsigned char*)malloc(S * S * 4);
    if (!img) return 0;
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            int i = (y * S + x) * 4;
            int n = (x * 11 + y * 7) % 23;
            unsigned char v = (unsigned char)(160 + n);
            img[i] = v; img[i+1] = (unsigned char)(v - 5); img[i+2] = (unsigned char)(v - 15); img[i+3] = 255;
        }
    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, S, S, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    free(img);
    return tex;
}

/* Bež kožna tekstura za 3D ruku */
static unsigned int createSkinTexture() {
    const int S = 64;
    unsigned char* img = (unsigned char*)malloc(S * S * 4);
    if (!img) return 0;
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            int i = (y * S + x) * 4;
            img[i] = (unsigned char)(220 + (rand() % 20));
            img[i+1] = (unsigned char)(180 + (rand() % 25));
            img[i+2] = (unsigned char)(160 + (rand() % 20));
            img[i+3] = 255;
        }
    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, S, S, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    free(img);
    return tex;
}

/* Tamna pozadina za student overlay ako nema student.png */
static unsigned int createStudentOverlayTexture() {
    const int W = 256, H = 64;
    unsigned char* img = (unsigned char*)malloc(W * H * 4);
    if (!img) return 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int i = (y * W + x) * 4;
            img[i] = 30; img[i+1] = 30; img[i+2] = 40; img[i+3] = 180;
        }
    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    free(img);
    return tex;
}

/* Kocka sa pozicijom, bojom, UV i normalom po strani (za zgrade i sat) */
static void createCubeVAO() {
    float s = 0.5f;
    float v[] = {
        -s,-s,s, 1,1,1,1, 0,0, 0,0,1,  s,-s,s, 1,1,1,1, 1,0, 0,0,1,  s,s,s, 1,1,1,1, 1,1, 0,0,1, -s,s,s, 1,1,1,1, 0,1, 0,0,1,
        -s,-s,-s, 1,1,1,1, 0,0, 0,0,-1, -s,s,-s, 1,1,1,1, 0,1, 0,0,-1, s,s,-s, 1,1,1,1, 1,1, 0,0,-1, s,-s,-s, 1,1,1,1, 1,0, 0,0,-1,
        -s,s,-s, 1,1,1,1, 0,0, 0,1,0, -s,s,s, 1,1,1,1, 0,1, 0,1,0, s,s,s, 1,1,1,1, 1,1, 0,1,0, s,s,-s, 1,1,1,1, 1,0, 0,1,0,
        -s,-s,-s, 1,1,1,1, 0,0, 0,-1,0, s,-s,-s, 1,1,1,1, 1,0, 0,-1,0, s,-s,s, 1,1,1,1, 1,1, 0,-1,0, -s,-s,s, 1,1,1,1, 0,1, 0,-1,0,
        s,-s,-s, 1,1,1,1, 0,0, 1,0,0, s,s,-s, 1,1,1,1, 0,1, 1,0,0, s,s,s, 1,1,1,1, 1,1, 1,0,0, s,-s,s, 1,1,1,1, 1,0, 1,0,0,
        -s,-s,-s, 1,1,1,1, 0,0, -1,0,0, -s,-s,s, 1,1,1,1, 1,0, -1,0,0, -s,s,s, 1,1,1,1, 1,1, -1,0,0, -s,s,-s, 1,1,1,1, 0,1, -1,0,0
    };
    unsigned int stride = 12 * sizeof(float);
    glGenVertexArrays(1, &VAO_cube);
    glBindVertexArray(VAO_cube);
    unsigned int VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(7*sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(9*sizeof(float)));
    glEnableVertexAttribArray(3);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

/* Quad preko celog ekrana za pozadinu (nebo) */
static void createFullscreenVAO() {
    float v[] = { -1.0f,-1.0f, 0,0,  1.0f,-1.0f, 1,0,  1.0f,1.0f, 1,1,  -1.0f,1.0f, 0,1 };
    glGenVertexArrays(1, &VAO_fullscreen);
    glBindVertexArray(VAO_fullscreen);
    unsigned int VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

/* Mali quad za overlay / druge 2D stvari (dinamički podaci u VBO). */
static void createQuadVAO() {
    float v[] = { -0.5f,-0.5f,0,0, 0.5f,-0.5f,1,0, 0.5f,0.5f,1,1, -0.5f,0.5f,0,1 };
    glGenVertexArrays(1, &VAO_quad);
    glBindVertexArray(VAO_quad);
    glGenBuffers(1, &VBO_quad);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_quad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

/* Jedan segment puta: pravougaonik u XZ (širina 80, dužina GROUND_SEGMENT_LENGTH) */
static void createGroundVAO() {
    float w = 80.0f;
    float len = GROUND_SEGMENT_LENGTH;
    float uvScale = 15.0f;
    float v[] = {
        0,0,0, 1,1,1,1, 0,0, 0,1,0,  w,0,0, 1,1,1,1, uvScale,0, 0,1,0,  w,0,-len, 1,1,1,1, uvScale,uvScale, 0,1,0,  0,0,-len, 1,1,1,1, 0,uvScale, 0,1,0
    };
    glGenVertexArrays(1, &VAO_ground);
    glBindVertexArray(VAO_ground);
    unsigned int VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 12*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 12*sizeof(float), (void*)(7*sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 12*sizeof(float), (void*)(9*sizeof(float)));
    glEnableVertexAttribArray(3);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

/* Quad na kome se u 3D prikazuje tekstura ekrana sata (FBO) */
static void createScreen3DVAO() {
    float s = 0.5f;
    float v[] = {
        -s,-s,0, 1,1,1,1, 1,0, 0,0,1,  s,-s,0, 1,1,1,1, 0,0, 0,0,1,  s,s,0, 1,1,1,1, 0,1, 0,0,1, -s,s,0, 1,1,1,1, 1,1, 0,0,1
    };
    unsigned int stride = 12 * sizeof(float);
    glGenVertexArrays(1, &VAO_screen3D);
    glBindVertexArray(VAO_screen3D);
    unsigned int VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(7*sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(9*sizeof(float)));
    glEnableVertexAttribArray(3);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

/* Mali quad u uglu za student overlay (ime, indeks). */
static void createOverlayVAO() {
    float v[] = { -1,-1,0,0,  -0.7f,-1,1,0,  -0.7f,-0.85f,1,1,  -1,-0.85f,0,1 };
    glGenVertexArrays(1, &VAO_overlay);
    glBindVertexArray(VAO_overlay);
    unsigned int VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

/* Pomeranje miša menja pitch/yaw kamere (gledanje gore-dole) */
void Scene_MouseCallback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;
    if (cameraLocked) return;
    if (firstMouse) { lastMouseX = (float)xpos; lastMouseY = (float)ypos; firstMouse = false; }
    float dy = (float)ypos - lastMouseY;
    lastMouseX = (float)xpos;
    lastMouseY = (float)ypos;
    cameraPitch -= dy * 0.15f;
    if (cameraPitch > 89.0f) cameraPitch = 89.0f;
    if (cameraPitch < -89.0f) cameraPitch = -89.0f;
    glm::vec3 front;
    front.x = cos(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
    front.y = sin(glm::radians(cameraPitch));
    front.z = sin(glm::radians(cameraYaw)) * cos(glm::radians(cameraPitch));
    cameraFront = glm::normalize(front);
}

void Scene_SetCameraLocked(bool locked) { cameraLocked = locked; }
void Scene_SetFirstMouse(bool first) { firstMouse = first; }

unsigned int Scene_GetShaderScreen(void) { return shaderScreen; }
unsigned int Scene_GetVAOQuad(void) { return VAO_quad; }
unsigned int Scene_GetVBOQuad(void) { return VBO_quad; }

/* Učitava šadere, VAO-ove, teksture (pod, zgrade, nebo, ruka, sat, student), shadow map */
void Scene_Init(unsigned int screenWidth, unsigned int screenHeight) {
    (void)screenWidth;
    (void)screenHeight;
    shaderMain = createShader("basic.vert", "basic.frag");
    shaderScreen = createShader("screen.vert", "screen.frag");
    shaderOverlay = createShader("overlay.vert", "overlay.frag");
    shaderDepth = createShader("depth.vert", "depth.frag");
    createCubeVAO();
    createQuadVAO();
    createFullscreenVAO();
    createGroundVAO();
    createScreen3DVAO();
    createOverlayVAO();

    texGround = preprocessTexture("res/grass.png");
    if (texGround == 0) texGround = createGrassTexture();
    texBuilding = loadImageToTexture("res/building.png");
    if (texBuilding == 0) texBuilding = loadImageToTexture("res/building.jpeg");
    if (texBuilding != 0) {
        glBindTexture(GL_TEXTURE_2D, texBuilding);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else
        texBuilding = createBuildingTexture();
    texSkin = createSkinTexture();
    texWatchCase = createWatchCaseTexture();
    texSky = loadImageToTexture("res/ground.jpg");
    if (texSky != 0) {
        glBindTexture(GL_TEXTURE_2D, texSky);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    VAO_hand = loadOBJToVAO("res/11537_Hand_v3.obj", &handVertexCount);
    if (VAO_hand == 0) VAO_hand = loadOBJToVAO("res/hand.obj", &handVertexCount);
    if (VAO_hand != 0) std::cout << "3D model ruke ucitan (" << handVertexCount << " trouglova)\n";
    texStudent = loadImageToTexture("res/student.png");
    if (texStudent == 0) texStudent = createStudentOverlayTexture();
    glBindTexture(GL_TEXTURE_2D, texStudent);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    /* Shadow map za senke od glavnog svetla (zgrade/ruka na put). */
    if (shaderDepth != 0) {
        glGenTextures(1, &shadowMapTex);
        glBindTexture(GL_TEXTURE_2D, shadowMapTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float border[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
        glGenFramebuffers(1, &shadowFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMapTex, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            shadowFBO = 0;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

/* Crta celu scenu: shadow pass, pozadina, pod, zgrade, ruka, sat, overlay sata, student */
void Scene_Render(double dt, bool running, float runBlend, double runBobPhase, float groundOffsetZ,
    bool watchInFront, bool depthTestEnabled, bool cullFaceEnabled,
    unsigned int screenWidth, unsigned int screenHeight,
    unsigned int texWatchFBO, unsigned int texWatchFrame, unsigned int fboWidth, unsigned int fboHeight)
{
    (void)dt;
    /* Normalizuj offset puta da uvek ima segment ispod kamere */
    groundOffsetZ = fmodf(groundOffsetZ, GROUND_CYCLE_LENGTH);
    if (groundOffsetZ < 0.0f) groundOffsetZ += GROUND_CYCLE_LENGTH;

    glViewport(0, 0, (int)screenWidth, (int)screenHeight);
    glClearColor(0.08f, 0.09f, 0.16f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (depthTestEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (cullFaceEnabled) { glEnable(GL_CULL_FACE); glCullFace(GL_BACK); } else glDisable(GL_CULL_FACE);

    float aspect = (float)screenWidth / (float)screenHeight;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 200.0f);
    glm::vec3 viewPos = cameraPos;
    viewPos.y += runBlend * 0.5f * (float)sin(runBobPhase);  /* prelaz pri puštanju D */
    glm::mat4 view = glm::lookAt(viewPos, viewPos + cameraFront, cameraUp);
    glm::vec3 lightPos(0.0f, 18.0f, 0.0f);
    float handAngleY;
    glm::vec3 handPos;
    if (watchInFront) {
        handPos = viewPos + cameraFront * RAISED_ARM_DIST;
        handPos.y -= 0.12f;
        handAngleY = (float)(atan2(-(double)cameraFront.z, -(double)cameraFront.x) * 180.0 / 3.14159265);
    } else {
        float hy = 1.5f - 0.1f * runBlend + runBlend * 0.18f * (float)sin(runBobPhase);  /* ruka: prelaz pri puštanju D */
        handPos = glm::vec3(watchWorldX, hy, watchWorldZ);
        handAngleY = 0.0f;
    }
    float watchZ = watchInFront ? WATCH_OFFSET_Z_RAISED : WATCH_OFFSET_Z_NORMAL;
    glm::vec3 watchPos = handPos
        + glm::vec3(cos(glm::radians(handAngleY)), 0.f, sin(glm::radians(handAngleY))) * WATCH_OFFSET_ON_HAND
        + glm::vec3(WATCH_OFFSET_X, WATCH_OFFSET_Y, watchZ);
    if (!watchInFront && VAO_hand != 0 && handVertexCount > 0)
        watchPos = handPos + glm::vec3(0.04f, 0.02f, 0.02f);

    /* Prvi prolaz: shadow map (crtamo pod, zgrade, ruku, sat iz perspektive svetla). */
    glm::mat4 lightSpaceMatrix(1.0f);
    if (shadowFBO != 0 && shaderDepth != 0) {
        glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f, 0.0f, -30.0f), glm::vec3(0.0f, 0.0f, -1.0f));
        glm::mat4 lightProj = glm::ortho(-55.0f, 15.0f, -55.0f, 15.0f, 0.5f, 70.0f);
        lightSpaceMatrix = lightProj * lightView;
        glViewport(0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderDepth);
        glUniformMatrix4fv(glGetUniformLocation(shaderDepth, "uLightVP"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
        glEnable(GL_DEPTH_TEST);
        bool cullOn = (cullFaceEnabled != 0);
        if (cullOn) glDisable(GL_CULL_FACE);
        for (int i = 0; i < GROUND_SEGMENTS; i++) {
            float segZ = -i * GROUND_SEGMENT_LENGTH + groundOffsetZ;
            if (segZ > GROUND_RESET_Z) segZ -= GROUND_CYCLE_LENGTH;
            glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(-40.0f, 0.0f, segZ));
            glUniformMatrix4fv(glGetUniformLocation(shaderDepth, "uM"), 1, GL_FALSE, glm::value_ptr(M));
            glBindVertexArray(VAO_ground);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }
        for (int i = 0; i < GROUND_SEGMENTS; i++) {
            float segZ = -i * GROUND_SEGMENT_LENGTH + groundOffsetZ;
            if (segZ > GROUND_RESET_Z) segZ -= GROUND_CYCLE_LENGTH;
            glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(5.0f + (i % 2) * 2.0f, 2.0f, segZ));
            M = glm::scale(M, glm::vec3(3.0f, 6.0f, 3.0f));
            glUniformMatrix4fv(glGetUniformLocation(shaderDepth, "uM"), 1, GL_FALSE, glm::value_ptr(M));
            glBindVertexArray(VAO_cube);
            for (int f = 0; f < 6; f++) glDrawArrays(GL_TRIANGLE_FAN, f * 4, 4);
        }
        for (int i = 0; i < GROUND_SEGMENTS; i++) {
            float segZ = -i * GROUND_SEGMENT_LENGTH + groundOffsetZ;
            if (segZ > GROUND_RESET_Z) segZ -= GROUND_CYCLE_LENGTH;
            glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(-5.0f - (i % 2) * 2.0f, 2.0f, segZ));
            M = glm::scale(M, glm::vec3(3.0f, 6.0f, 3.0f));
            glUniformMatrix4fv(glGetUniformLocation(shaderDepth, "uM"), 1, GL_FALSE, glm::value_ptr(M));
            glBindVertexArray(VAO_cube);
            for (int f = 0; f < 6; f++) glDrawArrays(GL_TRIANGLE_FAN, f * 4, 4);
        }
        glm::mat4 baseRd = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0, 1, 0));
        glm::mat4 Rfaced = glm::rotate(glm::mat4(1.0f), glm::radians(handAngleY), glm::vec3(0, 1, 0));
        glm::mat4 Rpalmad = watchInFront ? glm::mat4(1.0f) : glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(8, 1, 0));
        float handScaleD = watchInFront ? 0.055f : 0.14f;
        if (VAO_hand != 0 && handVertexCount > 0) {
            glm::mat4 Mhand = glm::translate(glm::mat4(1.0f), handPos) * Rfaced * baseRd * Rpalmad;
            Mhand = glm::scale(Mhand, glm::vec3(handScaleD, handScaleD, handScaleD));
            glUniformMatrix4fv(glGetUniformLocation(shaderDepth, "uM"), 1, GL_FALSE, glm::value_ptr(Mhand));
            glBindVertexArray(VAO_hand);
            glDrawArrays(GL_TRIANGLES, 0, handVertexCount);
        }
        float watchSizeD = watchInFront ? 0.18f : 0.58f;
        glm::mat4 Mwatch = glm::translate(glm::mat4(1.0f), watchPos);
        Mwatch = Mwatch * glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0, 1, 0));
        Mwatch = glm::scale(Mwatch, glm::vec3(watchSizeD * 0.425f, watchSizeD * 0.85f, watchSizeD * 0.85f));
        glUniformMatrix4fv(glGetUniformLocation(shaderDepth, "uM"), 1, GL_FALSE, glm::value_ptr(Mwatch));
        glBindVertexArray(VAO_cube);
        for (int f = 0; f < 6; f++) glDrawArrays(GL_TRIANGLE_FAN, f * 4, 4);
        if (cullOn) { glEnable(GL_CULL_FACE); glCullFace(GL_BACK); }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, (int)screenWidth, (int)screenHeight);
    }

    /* Pozadina (nebo) – fullscreen quad sa ground.jpg */
    if (texSky != 0 && VAO_fullscreen != 0) {
        glDisable(GL_DEPTH_TEST);
        glUseProgram(shaderScreen);
        glUniform1f(glGetUniformLocation(shaderScreen, "uEllipseRx"), 0.0f);
        glUniform1f(glGetUniformLocation(shaderScreen, "uEllipseRy"), 0.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texSky);
        glUniform1i(glGetUniformLocation(shaderScreen, "uTex"), 0);
        glUniform1f(glGetUniformLocation(shaderScreen, "uAlpha"), 1.0f);
        glUniform1i(glGetUniformLocation(shaderScreen, "uUseColor"), 0);
        glm::mat4 ortho = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shaderScreen, "uP"), 1, GL_FALSE, glm::value_ptr(ortho));
        glBindVertexArray(VAO_fullscreen);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
    }

    /* Glavni 3D prolaz: basic šader sa svetlom, senkama; crtamo pod, zgrade, ruku, sat. */
    glUseProgram(shaderMain);
    glUniform1i(glGetUniformLocation(shaderMain, "uUnlit"), 0);
    glUniformMatrix4fv(glGetUniformLocation(shaderMain, "uP"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(shaderMain, "uV"), 1, GL_FALSE, glm::value_ptr(view));
    glUniform3fv(glGetUniformLocation(shaderMain, "uViewPos"), 1, glm::value_ptr(viewPos));
    glm::vec3 lightKA(0.18f, 0.18f, 0.2f);
    glm::vec3 lightKD(0.85f, 0.82f, 0.78f);   /* blago toplo – “sunce odozgo” */
    glm::vec3 lightKS(0.95f, 0.92f, 0.88f);
    glUniform3fv(glGetUniformLocation(shaderMain, "uLightPos"), 1, glm::value_ptr(lightPos));
    glUniform3fv(glGetUniformLocation(shaderMain, "uLightKA"), 1, glm::value_ptr(lightKA));
    glUniform3fv(glGetUniformLocation(shaderMain, "uLightKD"), 1, glm::value_ptr(lightKD));
    glUniform3fv(glGetUniformLocation(shaderMain, "uLightKS"), 1, glm::value_ptr(lightKS));
    glUniform1f(glGetUniformLocation(shaderMain, "uShine"), 64.0f);
    glUniformMatrix4fv(glGetUniformLocation(shaderMain, "uLightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowMapTex);
    glUniform1i(glGetUniformLocation(shaderMain, "uShadowMap"), 1);

    /* Slabi izvor – ekran sata (handPos, watchPos iz ranijeg bloka). */
    glm::vec3 light2KA(0.06f, 0.10f, 0.18f);
    glm::vec3 light2KD(0.15f, 0.25f, 0.45f);
    glm::vec3 light2KS(0.08f, 0.14f, 0.25f);
    glUniform3fv(glGetUniformLocation(shaderMain, "uLight2Pos"), 1, glm::value_ptr(watchPos));
    glUniform3fv(glGetUniformLocation(shaderMain, "uLight2KA"), 1, glm::value_ptr(light2KA));
    glUniform3fv(glGetUniformLocation(shaderMain, "uLight2KD"), 1, glm::value_ptr(light2KD));
    glUniform3fv(glGetUniformLocation(shaderMain, "uLight2KS"), 1, glm::value_ptr(light2KS));
    glUniform1i(glGetUniformLocation(shaderMain, "uLight2On"), 1);
    glUniform1i(glGetUniformLocation(shaderMain, "useTex"), 1);
    glUniform1i(glGetUniformLocation(shaderMain, "transparent"), 0);

    /* Podloga: normala (0,1,0), kamera iznad – pri uklj. testu dubine uklj./isklj. naličja ne menja prikaz. */
    for (int i = 0; i < GROUND_SEGMENTS; i++) {
        float segZ = -i * GROUND_SEGMENT_LENGTH + groundOffsetZ;
        if (segZ > GROUND_RESET_Z) segZ -= GROUND_CYCLE_LENGTH;
        glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(-40.0f, 0.0f, segZ));
        glUniformMatrix4fv(glGetUniformLocation(shaderMain, "uM"), 1, GL_FALSE, glm::value_ptr(M));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texGround);
        glBindVertexArray(VAO_ground);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    /* Jedna zgrada na svaki segment puta – put i zgrade uvek zajedno */
    for (int i = 0; i < GROUND_SEGMENTS; i++) {
        float segZ = -i * GROUND_SEGMENT_LENGTH + groundOffsetZ;
        if (segZ > GROUND_RESET_Z) segZ -= GROUND_CYCLE_LENGTH;
        glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(5.0f + (i % 2) * 2.0f, 2.0f, segZ));
        M = glm::scale(M, glm::vec3(3.0f, 6.0f, 3.0f));
        glUniformMatrix4fv(glGetUniformLocation(shaderMain, "uM"), 1, GL_FALSE, glm::value_ptr(M));
        glBindTexture(GL_TEXTURE_2D, texBuilding);
        glBindVertexArray(VAO_cube);
        for (int f = 0; f < 6; f++) glDrawArrays(GL_TRIANGLE_FAN, f * 4, 4);
    }
    for (int i = 0; i < GROUND_SEGMENTS; i++) {
        float segZ = -i * GROUND_SEGMENT_LENGTH + groundOffsetZ;
        if (segZ > GROUND_RESET_Z) segZ -= GROUND_CYCLE_LENGTH;
        glm::mat4 M = glm::translate(glm::mat4(1.0f), glm::vec3(-5.0f - (i % 2) * 2.0f, 2.0f, segZ));
        M = glm::scale(M, glm::vec3(3.0f, 6.0f, 3.0f));
        glUniformMatrix4fv(glGetUniformLocation(shaderMain, "uM"), 1, GL_FALSE, glm::value_ptr(M));
        glBindTexture(GL_TEXTURE_2D, texBuilding);
        glBindVertexArray(VAO_cube);
        for (int f = 0; f < 6; f++) glDrawArrays(GL_TRIANGLE_FAN, f * 4, 4);
    }

    const float watchSize = watchInFront ? 0.18f : 0.58f;
    const float handScale = watchInFront ? 0.055f : 0.14f;
    glm::mat4 baseR = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0, 1, 0));
    glm::mat4 Rface = glm::rotate(glm::mat4(1.0f), glm::radians(handAngleY), glm::vec3(0, 1, 0));
    glm::mat4 Rpalma = watchInFront ? glm::mat4(1.0f) : glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(8, 1, 0));

    if (VAO_hand != 0 && handVertexCount > 0) {
        glm::mat4 Mhand = glm::translate(glm::mat4(1.0f), handPos);
        Mhand = Mhand * Rface * baseR * Rpalma;
        Mhand = glm::scale(Mhand, glm::vec3(handScale, handScale, handScale));
        glBindTexture(GL_TEXTURE_2D, texSkin);
        glUniformMatrix4fv(glGetUniformLocation(shaderMain, "uM"), 1, GL_FALSE, glm::value_ptr(Mhand));
        glBindVertexArray(VAO_hand);
        glDrawArrays(GL_TRIANGLES, 0, handVertexCount);
    } else {
        glBindTexture(GL_TEXTURE_2D, texSkin);
        glBindVertexArray(VAO_cube);
        float hx = handPos.x, hy = handPos.y, hz = handPos.z;
        float hs = watchInFront ? 0.35f : 0.5f;
        glm::mat4 Mforearm = glm::translate(glm::mat4(1.0f), glm::vec3(hx - 0.18f*hs, hy, hz));
        Mforearm = Mforearm * Rface * baseR * Rpalma;
        Mforearm = glm::scale(Mforearm, glm::vec3(0.22f*hs, 0.07f*hs, 0.05f*hs));
        glUniformMatrix4fv(glGetUniformLocation(shaderMain, "uM"), 1, GL_FALSE, glm::value_ptr(Mforearm));
        for (int f = 0; f < 6; f++) glDrawArrays(GL_TRIANGLE_FAN, f*4, 4);
        glm::mat4 Mwrist = glm::translate(glm::mat4(1.0f), glm::vec3(hx + 0.02f*hs, hy, hz));
        Mwrist = Mwrist * Rface * baseR * Rpalma;
        Mwrist = glm::scale(Mwrist, glm::vec3(0.04f*hs, 0.055f*hs, 0.05f*hs));
        glUniformMatrix4fv(glGetUniformLocation(shaderMain, "uM"), 1, GL_FALSE, glm::value_ptr(Mwrist));
        for (int f = 0; f < 6; f++) glDrawArrays(GL_TRIANGLE_FAN, f*4, 4);
        glm::mat4 Mpalm = glm::translate(glm::mat4(1.0f), glm::vec3(hx + 0.12f*hs, hy + 0.01f, hz));
        Mpalm = Mpalm * Rface * baseR * Rpalma;
        Mpalm = glm::scale(Mpalm, glm::vec3(0.06f*hs, 0.08f*hs, 0.025f*hs));
        glUniformMatrix4fv(glGetUniformLocation(shaderMain, "uM"), 1, GL_FALSE, glm::value_ptr(Mpalm));
        for (int f = 0; f < 6; f++) glDrawArrays(GL_TRIANGLE_FAN, f*4, 4);
    }

    if (!watchInFront) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (cullFaceEnabled) glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glm::mat4 Mband = glm::translate(glm::mat4(1.0f), handPos);
        Mband = Mband * glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0, 1, 0));
        Mband = glm::scale(Mband, glm::vec3(0.22f, 0.07f, 0.14f));
        glBindTexture(GL_TEXTURE_2D, texWatchCase);
        glUniformMatrix4fv(glGetUniformLocation(shaderMain, "uM"), 1, GL_FALSE, glm::value_ptr(Mband));
        glBindVertexArray(VAO_cube);
        for (int f = 0; f < 6; f++) glDrawArrays(GL_TRIANGLE_FAN, f*4, 4);
        if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
        glm::mat4 Mwatch = glm::translate(glm::mat4(1.0f), watchPos);
        Mwatch = Mwatch * glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0, 1, 0));
        float box = watchSize * 0.85f;
        Mwatch = glm::scale(Mwatch, glm::vec3(box * 0.5f, box, box));
        glUniformMatrix4fv(glGetUniformLocation(shaderMain, "uM"), 1, GL_FALSE, glm::value_ptr(Mwatch));
        for (int f = 0; f < 6; f++) glDrawArrays(GL_TRIANGLE_FAN, f*4, 4);
        if (cullFaceEnabled) { glEnable(GL_CULL_FACE); glCullFace(GL_BACK); }
    } else {
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        if (cullFaceEnabled) glDisable(GL_CULL_FACE);
        glm::mat4 Mwatch = glm::translate(glm::mat4(1.0f), watchPos);
        Mwatch = Mwatch * Rface * baseR * Rpalma;
        float box = watchSize * 0.85f;
        Mwatch = glm::scale(Mwatch, glm::vec3(box * 0.5f, box, box));
        glBindTexture(GL_TEXTURE_2D, texWatchCase);
        glUniformMatrix4fv(glGetUniformLocation(shaderMain, "uM"), 1, GL_FALSE, glm::value_ptr(Mwatch));
        for (int f = 0; f < 6; f++) glDrawArrays(GL_TRIANGLE_FAN, f*4, 4);
        glUniform1i(glGetUniformLocation(shaderMain, "uLight2On"), 1);
        glm::mat4 Mscreen = Mwatch
            * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.5f))
            * glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0, 1, 0))
            * glm::scale(glm::mat4(1.0f), glm::vec3(watchSize*0.85f, watchSize*0.55f, 0.02f));
        glUniform1i(glGetUniformLocation(shaderMain, "uUnlit"), 1);
        bool wasCull = cullFaceEnabled;
        if (wasCull) glDisable(GL_CULL_FACE);
        glUniformMatrix4fv(glGetUniformLocation(shaderMain, "uM"), 1, GL_FALSE, glm::value_ptr(Mscreen));
        glBindTexture(GL_TEXTURE_2D, texWatchFBO);
        glBindVertexArray(VAO_screen3D);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        if (wasCull) glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glUniform1i(glGetUniformLocation(shaderMain, "uUnlit"), 0);
        if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
        if (cullFaceEnabled) { glEnable(GL_CULL_FACE); glCullFace(GL_BACK); }
    }

    if (watchInFront) {
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glm::mat4 ortho = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
        glUseProgram(shaderScreen);
        glUniform1i(glGetUniformLocation(shaderScreen, "uTex"), 0);
        glUniform1f(glGetUniformLocation(shaderScreen, "uAlpha"), 1.0f);
        glUniform1i(glGetUniformLocation(shaderScreen, "uUseColor"), 0);
        glUniformMatrix4fv(glGetUniformLocation(shaderScreen, "uP"), 1, GL_FALSE, glm::value_ptr(ortho));
        float ar = (float)fboHeight / (float)fboWidth;
        const float frameScale = 0.55f;
        float s = 0.5f * frameScale;
        float overlayV[] = { -s,-s*ar,0,0, s,-s*ar,1,0, s,s*ar,1,1, -s,s*ar,0,1 };
        glUniform1f(glGetUniformLocation(shaderScreen, "uEllipseRx"), s);
        glUniform1f(glGetUniformLocation(shaderScreen, "uEllipseRy"), s * ar);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_quad);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(overlayV), overlayV);
        glBindVertexArray(VAO_quad);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texWatchFBO);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        if (texWatchFrame) {
            glBindTexture(GL_TEXTURE_2D, texWatchFrame);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }
        glUniform1f(glGetUniformLocation(shaderScreen, "uEllipseRx"), 0.0f);
        glUniform1f(glGetUniformLocation(shaderScreen, "uEllipseRy"), 0.0f);
        glBindVertexArray(0);
        if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
    }

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (texStudent != 0) {
        glDisable(GL_CULL_FACE);
        glUseProgram(shaderScreen);
        glUniform1f(glGetUniformLocation(shaderScreen, "uEllipseRx"), 0.0f);
        glUniform1f(glGetUniformLocation(shaderScreen, "uEllipseRy"), 0.0f);
        glUniform1i(glGetUniformLocation(shaderScreen, "uTex"), 0);
        glUniform1f(glGetUniformLocation(shaderScreen, "uAlpha"), 1.0f);
        glUniform1i(glGetUniformLocation(shaderScreen, "uUseColor"), 0);
        glm::mat4 ortho = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shaderScreen, "uP"), 1, GL_FALSE, glm::value_ptr(ortho));
        float v[] = { -1.0f,-1.0f,0,0,  -0.7f,-1.0f,1,0,  -0.7f,-0.85f,1,1,  -1.0f,-0.85f,0,1 };
        glBindBuffer(GL_ARRAY_BUFFER, VBO_quad);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
        glBindVertexArray(VAO_quad);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texStudent);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glBindVertexArray(0);
        if (cullFaceEnabled) { glEnable(GL_CULL_FACE); glCullFace(GL_BACK); }
    }
    if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
}

void Scene_Shutdown(void) {
    glDeleteVertexArrays(1, &VAO_cube);
    glDeleteVertexArrays(1, &VAO_quad);
    if (VAO_fullscreen) glDeleteVertexArrays(1, &VAO_fullscreen);
    glDeleteVertexArrays(1, &VAO_ground);
    glDeleteVertexArrays(1, &VAO_screen3D);
    if (VAO_hand) glDeleteVertexArrays(1, &VAO_hand);
    glDeleteVertexArrays(1, &VAO_overlay);
    glDeleteProgram(shaderMain);
    glDeleteProgram(shaderScreen);
    glDeleteProgram(shaderOverlay);
    if (shaderDepth) glDeleteProgram(shaderDepth);
    if (shadowFBO) glDeleteFramebuffers(1, &shadowFBO);
    if (shadowMapTex) glDeleteTextures(1, &shadowMapTex);
}
