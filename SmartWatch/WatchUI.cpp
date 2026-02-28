#define _CRT_SECURE_NO_WARNINGS
/*
 * WatchUI – 2D ekran sata u FBO: vreme, BPM/EKG, baterija
 * Crta sadržaj u teksturu koju Scene lepi na 3D model sata; ažurira BPM i EKG (D = trčanje)
 */
#include "WatchUI.h"
#include "Util.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <iostream>

static WatchScreen currentScreen = SCREEN_TIME;
static int clockHours = 0, clockMinutes = 0, clockSeconds = 0;
static double clockAccum = 0.0;
static int bpm = 70;
static double bpmAccum = 0.0;
static float batteryPercent = 100.0f;
static double batteryAccum = 0.0;
static double ekgOffset = 0.0;
static float ekgCompressionFactor = 0.0f;
static bool showBpmWarning = false;

static unsigned int FBO_watch = 0;
static unsigned int texWatchFBO = 0;
static int fboWidth = 700;
static int fboHeight = 700;
static unsigned int texWatchDigit[10] = {0};
static unsigned int texArrowL = 0, texArrowR = 0, texEkg2d = 0, texWarning = 0;
static unsigned int texPercent = 0, texBpmLabel = 0, texWatchFrame = 0;
static unsigned int texDigits = 0, texEKG = 0;

static unsigned int s_shaderScreen = 0;
static unsigned int s_VAO_quad = 0;
static unsigned int s_VBO_quad = 0;

/* Učitava teksturu iz res/ i podešava wrap i filter. */
static unsigned int loadTexFromRes(const char* filename) {
    std::string path = "res/";
    path += filename;
    unsigned int t = loadImageToTexture(path.c_str());
    if (t == 0) return 0;
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return t;
}

/* Proceduralna tekstura cifara 0–9 i dvotačke (sedam segmenta). */
static unsigned int createDigitsTexture() {
    const int CW = 16, CH = 24, COLS = 12;
    unsigned char* img = (unsigned char*)malloc(COLS * CW * CH * 4);
    if (!img) return 0;
    for (int c = 0; c < COLS; c++) {
        for (int py = 0; py < CH; py++) {
            for (int px = 0; px < CW; px++) {
                int ix = c * CW + px;
                int i = (py * (COLS * CW) + ix) * 4;
                img[i] = img[i+1] = img[i+2] = 255;
                img[i+3] = 0;
                float x = (px - CW/2.0f) / (CW/2.0f);
                float y = (py - CH/2.0f) / (CH/2.0f);
                bool on = false;
                if (c <= 10) {
                    int d = c;
                    if (d == 10) {
                        on = (px >= CW/2-2 && px <= CW/2+2 && (py < CH/3 || py > 2*CH/3));
                    } else {
                        float seg[7];
                        seg[0] = (x >= -0.9f && x <= 0.9f && y >= 0.5f);
                        seg[1] = (x >= 0.5f && y >= -0.5f && y <= 0.5f);
                        seg[2] = (x >= 0.5f && y <= -0.5f);
                        seg[3] = (x >= -0.9f && x <= 0.9f && y <= -0.5f);
                        seg[4] = (x <= -0.5f && y <= -0.5f);
                        seg[5] = (x <= -0.5f && y >= -0.5f && y <= 0.5f);
                        seg[6] = (x >= -0.9f && x <= 0.9f && y >= -0.1f && y <= 0.1f);
                        bool s[10][7] = {
                            {1,1,1,1,1,1,0},{0,1,1,0,0,0,0},{1,1,0,1,1,0,1},{1,1,1,1,0,0,1},
                            {0,1,1,0,0,1,1},{1,0,1,1,0,1,1},{1,0,1,1,1,1,1},{1,1,1,0,0,0,0},
                            {1,1,1,1,1,1,1},{1,1,1,1,0,1,1}
                        };
                        for (int k = 0; k < 7 && d >= 0 && d <= 9; k++) if (s[d][k] && seg[k]) on = true;
                    }
                }
                if (on) img[i+3] = 255;
            }
        }
    }
    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, COLS * CW, CH, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    free(img);
    return tex;
}

/* Proceduralna EKG tekstura: tri pika koji se ponavljaju, bez ravnih linija (ako nema ekg.png). */
static unsigned int createEKGTexture() {
    const int W = 256, H = 64;
    unsigned char* img = (unsigned char*)malloc(W * H * 4);
    if (!img) return 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int i = (y * W + x) * 4;
            img[i] = img[i+1] = img[i+2] = 0;
            img[i+3] = 0;
        }
    /* Konstantno EKG: samo tri linije (tri pika) koji se ponavljaju, bez ravnih linija. */
    const int cyclePix = 80;
    for (int x = 0; x < W; x++) {
        float u = (float)(x % cyclePix) / (float)cyclePix;
        float yf;
        if (u < 0.2f)      yf = H/2.0f - 8.0f;
        else if (u < 0.4f) yf = H/2.0f + 18.0f;
        else if (u < 0.6f) yf = H/2.0f - 10.0f;
        else               yf = H/2.0f - 10.0f + (u - 0.6f) / 0.4f * 2.0f;  /* prelaz u sledeći ciklus, nema ravnu liniju */
        int y = (int)(yf + 0.5f);
        if (y >= 0 && y < H) {
            for (int dy = -2; dy <= 2; dy++)
                if (y+dy >= 0 && y+dy < H) {
                    int idx = ((y+dy)*W + x)*4;
                    img[idx] = 0; img[idx+1] = 255; img[idx+2] = 100; img[idx+3] = 255;
                }
        }
    }
    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    free(img);
    return tex;
}

/* Crtanje pravougaonika sa teksturom u FBO (koordinate -1..1). */
static void drawFboRect(float x, float y, float w, float h, unsigned int tex) {
    if (tex == 0) return;
    float v[] = { x, y, 0,0,  x+w, y, 1,0,  x+w, y+h, 1,1,  x, y+h, 0,1 };
    glBindTexture(GL_TEXTURE_2D, tex);
    glBindBuffer(GL_ARRAY_BUFFER, s_VBO_quad);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glBindVertexArray(s_VAO_quad);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

/* Crtanje obojenog pravougaonika (bez teksture). */
static void drawFboColor(float x, float y, float w, float h, float r, float g, float b) {
    glUniform1i(glGetUniformLocation(s_shaderScreen, "uUseColor"), 1);
    glUniform4f(glGetUniformLocation(s_shaderScreen, "uColor"), r, g, b, 1.0f);
    float v[] = { x, y, 0,0,  x+w, y, 1,0,  x+w, y+h, 1,1,  x, y+h, 0,1 };
    glBindBuffer(GL_ARRAY_BUFFER, s_VBO_quad);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glBindVertexArray(s_VAO_quad);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glUniform1i(glGetUniformLocation(s_shaderScreen, "uUseColor"), 0);
}

/* Jedna cifra na zadatoj poziciji (PNG ili proceduralna) */
static void drawFboDigit(int d, float x, float y, float size) {
    if (d < 0 || d > 9) return;
    unsigned int tex = (texWatchDigit[d] != 0) ? texWatchDigit[d] : texDigits;
    if (tex == 0) return;
    float u0, u1, v0, v1;
    if (texWatchDigit[d] != 0) {
        u0 = 0.0f; u1 = 1.0f;
        v0 = 0.0f; v1 = 1.0f;  /* PNG učitane sa vertical flip u Util */
    } else {
        const int COLS = 12;
        u0 = (float)d / COLS; u1 = (float)(d + 1) / COLS;
        v0 = 1.0f; v1 = 0.0f;
    }
    float v[] = { x, y, u0,v0,  x+size, y, u1,v0,  x+size, y+size*1.5f, u1,v1,  x, y+size*1.5f, u0,v1 };
    glBindTexture(GL_TEXTURE_2D, tex);
    glBindBuffer(GL_ARRAY_BUFFER, s_VBO_quad);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glBindVertexArray(s_VAO_quad);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

/* Iscrtava broj sa zadatim brojem cifara */
static void drawFboNumber(int num, float x, float y, float size, int digits) {
    float sp = size * 1.1f;
    for (int i = digits - 1; i >= 0; i--) {
        int p = 1; for (int k = 0; k < i; k++) p *= 10;
        drawFboDigit((num / p) % 10, x, y, size);
        x += sp;
    }
}

/* EKG traka: tekstura (PNG ili proceduralna) */
static void drawFboEKG(float x, float y, float w, float h, float offset, float scale) {
    unsigned int tex = (texEkg2d != 0) ? texEkg2d : texEKG;
    if (tex == 0) return;
    float v[] = { x, y, offset, 0,  x+w, y, offset+scale, 0,  x+w, y+h, offset+scale, 1,  x, y+h, offset, 1 };
    glBindTexture(GL_TEXTURE_2D, tex);
    glBindBuffer(GL_ARRAY_BUFFER, s_VBO_quad);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glBindVertexArray(s_VAO_quad);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

/* Postavlja trenutno vreme sata */
void WatchUI_SetTime(int hours, int minutes, int seconds) {
    clockHours = hours;
    clockMinutes = minutes;
    clockSeconds = seconds;
}

/* Inicijalizacija: šader i quad od Scene, učitavanje tekstura, kreiranje FBO za ekran sata. */
void WatchUI_Init(unsigned int shaderScreen, unsigned int VAO_quad, unsigned int VBO_quad,
    unsigned int screenWidth, unsigned int screenHeight)
{
    (void)screenWidth;
    (void)screenHeight;
    s_shaderScreen = shaderScreen;
    s_VAO_quad = VAO_quad;
    s_VBO_quad = VBO_quad;

    texDigits = createDigitsTexture();
    texEKG = createEKGTexture();
    for (int i = 0; i < 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "digit%d.png", i);
        texWatchDigit[i] = loadTexFromRes(name);
    }
    texArrowL = loadTexFromRes("arrow_left.png");
    texArrowR = loadTexFromRes("arrow_right.png");
    texEkg2d = loadTexFromRes("ekg.png");
    if (texEkg2d != 0) {
        glBindTexture(GL_TEXTURE_2D, texEkg2d);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); 
    }
    texWarning = loadTexFromRes("warning.png");
    texPercent = loadTexFromRes("percent.png");
    texBpmLabel = loadTexFromRes("bpm_label.png");
    texWatchFrame = loadTexFromRes("watch_frame.png");

    glGenFramebuffers(1, &FBO_watch);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO_watch);
    glGenTextures(1, &texWatchFBO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texWatchFBO);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fboWidth, fboHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texWatchFBO, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "FBO greska\n";
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

/* Ažurira vreme, BPM, bateriju i EKG offset (BPM raste kad držiš D na ekranu srca, opada kad pustiš) */
void WatchUI_Update(double dt, bool keyDPressed) {
    clockAccum += dt;
    if (clockAccum >= 1.0) { clockAccum -= 1.0; clockSeconds++; }
    if (clockSeconds >= 60) { clockSeconds = 0; clockMinutes++; }
    if (clockMinutes >= 60) { clockMinutes = 0; clockHours++; }
    if (clockHours >= 24) clockHours = 0;

    batteryAccum += dt;
    if (batteryAccum >= 10.0) { batteryAccum -= 10.0; batteryPercent -= 1.0f; if (batteryPercent < 0) batteryPercent = 0; }

    bpmAccum += dt;
    if (bpmAccum >= 1.0) {
        bpmAccum -= 1.0;
        if (keyDPressed && currentScreen == SCREEN_HEART) {
            bpm = bpm + (rand() % 5) + 2;
            if (bpm > 220) bpm = 220;
        } else {
            /* Postepeno smanjenje BPM pri puštanju D (isto kao povećanje pri držanju). */
            bpm -= 7;
            if (bpm < 60) bpm = 60 + (rand() % 21);
        }
    }
    showBpmWarning = (currentScreen == SCREEN_HEART && bpm > 200);

    if (keyDPressed && currentScreen == SCREEN_HEART) {
        ekgOffset -= 0.005f + (ekgCompressionFactor * 0.005f);
        if (ekgCompressionFactor < 3.0f) ekgCompressionFactor += 0.003f;
    } else {
        ekgOffset -= 0.003f;
        if (ekgCompressionFactor > 0.0f) ekgCompressionFactor -= 0.004f;
        if (ekgCompressionFactor < 0.0f) ekgCompressionFactor = 0.0f;
    }
    if (ekgOffset < -1.0f) ekgOffset = 0.0f;
}

/* Crta sadržaj ekrana sata u FBO (vreme / srce+BPM / baterija)  */
void WatchUI_RenderToFBO(double dt, unsigned int screenWidth, unsigned int screenHeight) {
    (void)dt;
    glBindFramebuffer(GL_FRAMEBUFFER, FBO_watch);
    glViewport(0, 0, fboWidth, fboHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glm::mat4 ortho = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
    glUseProgram(s_shaderScreen);
    glUniform1f(glGetUniformLocation(s_shaderScreen, "uEllipseRx"), 0.0f);
    glUniform1f(glGetUniformLocation(s_shaderScreen, "uEllipseRy"), 0.0f);
    glUniform1i(glGetUniformLocation(s_shaderScreen, "uTex"), 0);
    glUniform1f(glGetUniformLocation(s_shaderScreen, "uAlpha"), 1.0f);
    glUniform1i(glGetUniformLocation(s_shaderScreen, "uUseColor"), 0);
    glUniformMatrix4fv(glGetUniformLocation(s_shaderScreen, "uP"), 1, GL_FALSE, (const float*)&ortho[0][0]);
    glActiveTexture(GL_TEXTURE0);

    const float arFbo = (float)fboHeight / (float)fboWidth;
    const float innerW = 1.25f, innerH = 1.34f * arFbo;
    const float innerX = -0.5f * innerW, innerY = -0.5f * innerH;
    int bat = (int)batteryPercent;
    if (bat == 0) {
        drawFboColor(innerX, innerY, innerW, innerH, 0.1f, 0.1f, 0.15f);
    } else if (currentScreen == SCREEN_TIME) {
        drawFboColor(innerX, innerY, innerW, innerH, 0.1f, 0.1f, 0.15f);
        float ds = 0.12f, sp = ds * 1.1f, cw = 0.015f, cs = 0.01f;
        float total = 6.0f * sp + 2.0f * (cw + 2.0f * cs);
        float sx = -total / 2.0f;
        drawFboNumber(clockHours, sx, 0.0f, ds, 2);
        sx += 2.0f * sp + cs;
        drawFboColor(sx, 0.1f, cw, 0.03f, 1, 1, 1);
        drawFboColor(sx, 0.02f, cw, 0.03f, 1, 1, 1);
        sx += cw + cs;
        drawFboNumber(clockMinutes, sx, 0.0f, ds, 2);
        sx += 2.0f * sp + cs;  
        drawFboColor(sx, 0.1f, cw, 0.03f, 1, 1, 1);
        drawFboColor(sx, 0.02f, cw, 0.03f, 1, 1, 1);
        sx += cw + cs;
        drawFboNumber(clockSeconds, sx, 0.0f, ds, 2);
        if (texArrowR) drawFboRect(0.35f, -0.35f, 0.15f, 0.15f, texArrowR);
    } else if (currentScreen == SCREEN_HEART) {
        if (bpm >= 200 && showBpmWarning) {
            drawFboColor(innerX, innerY, innerW, innerH, 0.2f, 0.05f, 0.05f);
            if (texWarning) drawFboRect(-0.5f, -0.05f, 1.0f, 0.5f, texWarning);
            drawFboNumber(bpm, -0.225f, -0.15f, 0.15f, 3);
            if (texArrowL) drawFboRect(-0.5f, -0.35f, 0.15f, 0.15f, texArrowL);
            if (texArrowR) drawFboRect(0.35f, -0.35f, 0.15f, 0.15f, texArrowR);
        } else {
            drawFboColor(innerX, innerY, innerW, innerH, 0.15f, 0.06f, 0.08f);
            if (texBpmLabel) drawFboRect(-0.25f, 0.25f, 0.5f, 0.18f, texBpmLabel);
            drawFboNumber(bpm, -0.3f, 0.0f, 0.18f, 3);
            float ekgScale = 3.0f + ekgCompressionFactor;
            drawFboEKG(-0.5f, -0.35f, 1.0f, 0.2f, (float)ekgOffset, ekgScale);
            if (texArrowL) drawFboRect(-0.5f, -0.35f, 0.15f, 0.15f, texArrowL);
            if (texArrowR) drawFboRect(0.35f, -0.35f, 0.15f, 0.15f, texArrowR);
        }
    } else {
        drawFboColor(innerX, innerY, innerW, innerH, 0.06f, 0.1f, 0.18f);
        float ds = 0.18f, sp = ds * 1.1f, pw = ds * 0.8f;
        int dc = (bat >= 100) ? 3 : (bat >= 10) ? 2 : 1;
        float total = (float)dc * sp + pw;
        float sx = -total / 2.0f;
        drawFboNumber(bat, sx, 0.15f, ds, dc);
        if (texPercent) drawFboRect(sx + (float)dc * sp, 0.15f, pw, ds * 1.5f, texPercent);
        float bw = 0.7f, bh = 0.25f, bx = -0.35f, by = -0.25f;
        drawFboColor(bx, by, bw, 0.02f, 1, 1, 1);
        drawFboColor(bx, by + bh, bw, 0.02f, 1, 1, 1);
        drawFboColor(bx, by, 0.02f, bh, 1, 1, 1);
        drawFboColor(bx + bw, by, 0.02f, bh, 1, 1, 1);
        float fillW = (bw - 0.08f) * (batteryPercent / 100.0f);
        float fillX = bx + bw - 0.04f - fillW;
        float r = batteryPercent > 20.0f ? 0.0f : (batteryPercent > 10.0f ? 1.0f : 1.0f);
        float g = batteryPercent > 20.0f ? 1.0f : (batteryPercent > 10.0f ? 1.0f : 0.0f);
        float bl = batteryPercent > 20.0f ? 0.0f : (batteryPercent > 10.0f ? 0.0f : 0.0f);
        drawFboColor(fillX, by + 0.04f, fillW, bh - 0.08f, r, g, bl);
        if (texArrowL) drawFboRect(-0.5f, -0.35f, 0.15f, 0.15f, texArrowL);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, (int)screenWidth, (int)screenHeight);
}

/* Klik levo/desno na strelice menja ekran sata (vreme → srce → baterija). */
void WatchUI_CheckArrowClick(GLFWwindow* window, unsigned int screenWidth, unsigned int screenHeight, bool watchInFront) {
    if (!watchInFront) return;
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    float nx = (float)(2.0 * mx / screenWidth - 1.0);
    float ny = (float)(1.0 - 2.0 * my / screenHeight);
    if (nx < 0.0f && ny > -0.5f && ny < 0.5f) {
        if (currentScreen == SCREEN_HEART) currentScreen = SCREEN_TIME;
        else if (currentScreen == SCREEN_BATTERY) currentScreen = SCREEN_HEART;
    } else if (nx > 0.0f && ny > -0.5f && ny < 0.5f) {
        if (currentScreen == SCREEN_TIME) currentScreen = SCREEN_HEART;
        else if (currentScreen == SCREEN_HEART) currentScreen = SCREEN_BATTERY;
    }
}

WatchScreen WatchUI_GetCurrentScreen(void) { return currentScreen; }
unsigned int WatchUI_GetTexWatchFBO(void) { return texWatchFBO; }
unsigned int WatchUI_GetTexWatchFrame(void) { return texWatchFrame; }
unsigned int WatchUI_GetFboWidth(void) { return (unsigned int)fboWidth; }
unsigned int WatchUI_GetFboHeight(void) { return (unsigned int)fboHeight; }

/* Oslobađa FBO i ostale resurse ekrana sata. */
void WatchUI_Shutdown(void) {
    glDeleteFramebuffers(1, &FBO_watch);
    /* teksture sata ne brišemo globalno – ostavljamo za jednostavnost; mogu se dodati glDeleteTextures ako treba */
}
