#define _CRT_SECURE_NO_WARNINGS
#include <GL/glew.h>
#include <cmath>
#include <cstdio>
#include <string>
#include "Util.h"
#include "Watch2D.h"

static unsigned int shaderProgram, colorShaderProgram;
static unsigned int VAOrect, VBOrect, VAOcolor, VBOcolor;
static unsigned int digitTextures[10];
static unsigned int arrowRightTex, arrowLeftTex, nameTex, ekgTex, warningTex, percentTex, bpmLabelTex, grassTex, watchFrameTex;

static void loadTex(unsigned int& out, const char* subpath, bool repeatS = false) {
    std::string path = std::string("C:/Users/Asus/Desktop/SmartWatch/Resources/") + subpath;
    out = loadImageToTexture(path.c_str());
    if (out == 0) out = loadImageToTexture((std::string("res/") + subpath).c_str());
    if (out == 0) return;
    glBindTexture(GL_TEXTURE_2D, out);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeatS ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void drawRect(float x, float y, float width, float height, unsigned int texture) {
    if (texture == 0) return;
    glUseProgram(shaderProgram);
    float vertices[] = { x, y, 0.0f, 0.0f,  x + width, y, 1.0f, 0.0f,  x + width, y + height, 1.0f, 1.0f,  x, y + height, 0.0f, 1.0f };
    glBindVertexArray(VAOrect);
    glBindBuffer(GL_ARRAY_BUFFER, VBOrect);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glBindTexture(GL_TEXTURE_2D, texture);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

static void drawEKG(float x, float y, float width, float height, float offset, float scale) {
    if (ekgTex == 0) return;
    glUseProgram(shaderProgram);
    float vertices[] = { x, y, offset, 0.0f,  x + width, y, offset + scale, 0.0f,  x + width, y + height, offset + scale, 1.0f,  x, y + height, offset, 1.0f };
    glBindVertexArray(VAOrect);
    glBindBuffer(GL_ARRAY_BUFFER, VBOrect);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glBindTexture(GL_TEXTURE_2D, ekgTex);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

static void drawColorRect(float x, float y, float width, float height, float r, float g, float b) {
    glUseProgram(colorShaderProgram);
    float vertices[] = { x, y,  x + width, y,  x + width, y + height,  x, y + height };
    glBindVertexArray(VAOcolor);
    glBindBuffer(GL_ARRAY_BUFFER, VBOcolor);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glUniform4f(glGetUniformLocation(colorShaderProgram, "uColor"), r, g, b, 1.0f);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

static void drawDigit(int digit, float x, float y, float size) {
    if (digit >= 0 && digit <= 9 && digitTextures[digit] != 0)
        drawRect(x, y, size, size * 1.5f, digitTextures[digit]);
}

static void drawNumber(int number, float x, float y, float size, int digitCount) {
    float spacing = size * 1.1f;
    for (int i = digitCount - 1; i >= 0; i--) {
        int digit = (number / (int)pow(10, i)) % 10;
        drawDigit(digit, x, y, size);
        x += spacing;
    }
}

static void drawClockScreen(int hours, int minutes, int seconds, int battery) {
    if (grassTex) drawRect(-1.0f, -1.0f, 2.0f, 2.0f, grassTex);
    if (nameTex) drawRect(-0.95f, 0.85f, 0.5f, 0.13f, nameTex);
    if (battery == 0) {
        drawColorRect(-0.48f, -0.52f, 0.98f, 0.99f, 0.0f, 0.0f, 0.0f);
    } else {
        drawColorRect(-0.48f, -0.52f, 0.98f, 0.99f, 0.1f, 0.1f, 0.15f);
        float digitSize = 0.12f;
        float digitSpacing = digitSize * 1.1f;
        float colonWidth = 0.015f;
        float colonSpacing = 0.01f;
        float totalWidth = 6 * digitSpacing + 2 * (colonWidth + 2 * colonSpacing);
        float startX = -totalWidth / 2.0f;
        drawNumber(hours, startX, 0.0f, digitSize, 2);
        startX += 2 * digitSpacing + colonSpacing;
        drawColorRect(startX, 0.1f, colonWidth, 0.03f, 1, 1, 1);
        drawColorRect(startX, 0.02f, colonWidth, 0.03f, 1, 1, 1);
        startX += colonWidth + colonSpacing;
        drawNumber(minutes, startX, 0.0f, digitSize, 2);
        startX += 2 * digitSpacing + colonSpacing;
        drawColorRect(startX, 0.1f, colonWidth, 0.03f, 1, 1, 1);
        drawColorRect(startX, 0.02f, colonWidth, 0.03f, 1, 1, 1);
        startX += colonWidth + colonSpacing;
        drawNumber(seconds, startX, 0.0f, digitSize, 2);
        if (arrowRightTex) drawRect(0.35f, -0.35f, 0.15f, 0.15f, arrowRightTex);
    }
    if (watchFrameTex) drawRect(-0.75f, -0.75f, 1.5f, 1.5f, watchFrameTex);
}

static void drawHeartScreen(int bpm, int battery, double ekgOffset, float ekgCompressionFactor, bool showWarning) {
    if (grassTex) drawRect(-1.0f, -1.0f, 2.0f, 2.0f, grassTex);
    if (nameTex) drawRect(-0.95f, 0.85f, 0.5f, 0.13f, nameTex);
    if (battery == 0) {
        drawColorRect(-0.48f, -0.52f, 0.98f, 0.99f, 0.0f, 0.0f, 0.0f);
    } else {
        if (bpm >= 200 && showWarning) {
            drawColorRect(-0.48f, -0.52f, 0.98f, 0.99f, 1.0f, 0.0f, 0.0f);
            if (warningTex) drawRect(-0.5f, -0.05f, 1.0f, 0.5f, warningTex);
            float digitSize = 0.15f;
            float digitSpacing = digitSize * 1.1f;
            float totalWidth = digitSpacing * 3;
            drawNumber(bpm, -totalWidth / 2.0f, -0.15f, digitSize, 3);
            if (arrowLeftTex) drawRect(-0.5f, -0.35f, 0.15f, 0.15f, arrowLeftTex);
            if (arrowRightTex) drawRect(0.35f, -0.35f, 0.15f, 0.15f, arrowRightTex);
        } else {
            drawColorRect(-0.48f, -0.52f, 0.98f, 0.99f, 0.15f, 0.05f, 0.05f);
            if (bpmLabelTex) drawRect(-0.25f, 0.25f, 0.5f, 0.18f, bpmLabelTex);
            drawNumber(bpm, -0.3f, 0.0f, 0.18f, 3);
            float ekgScale = 3.0f + ekgCompressionFactor;
            drawEKG(-0.5f, -0.35f, 1.0f, 0.2f, (float)ekgOffset, ekgScale);
            if (arrowLeftTex) drawRect(-0.5f, -0.35f, 0.15f, 0.15f, arrowLeftTex);
            if (arrowRightTex) drawRect(0.35f, -0.35f, 0.15f, 0.15f, arrowRightTex);
        }
    }
    if (watchFrameTex) drawRect(-0.75f, -0.75f, 1.5f, 1.5f, watchFrameTex);
}

static void drawBatteryScreen(int battery) {
    if (grassTex) drawRect(-1.0f, -1.0f, 2.0f, 2.0f, grassTex);
    if (nameTex) drawRect(-0.95f, 0.85f, 0.5f, 0.13f, nameTex);
    if (battery == 0) {
        drawColorRect(-0.48f, -0.52f, 0.98f, 0.99f, 0.0f, 0.0f, 0.0f);
    } else {
        drawColorRect(-0.48f, -0.52f, 0.98f, 0.99f, 0.05f, 0.1f, 0.15f);
        float digitSize = 0.18f;
        float digitSpacing = digitSize * 1.1f;
        float percentWidth = digitSize * 0.8f;
        if (battery == 100) {
            float totalWidth = 3 * digitSpacing + percentWidth;
            float startX = -totalWidth / 2.0f;
            drawNumber(battery, startX, 0.15f, digitSize, 3);
            if (percentTex) drawRect(startX + 3 * digitSpacing, 0.15f, percentWidth, digitSize * 1.5f, percentTex);
        } else if (battery >= 10) {
            float totalWidth = 2 * digitSpacing + percentWidth;
            float startX = -totalWidth / 2.0f;
            drawNumber(battery, startX, 0.15f, digitSize, 2);
            if (percentTex) drawRect(startX + 2 * digitSpacing, 0.15f, percentWidth, digitSize * 1.5f, percentTex);
        } else {
            float totalWidth = digitSpacing + percentWidth;
            float startX = -totalWidth / 2.0f;
            drawNumber(battery, startX, 0.15f, digitSize, 1);
            if (percentTex) drawRect(startX + digitSpacing, 0.15f, percentWidth, digitSize * 1.5f, percentTex);
        }
        float batteryWidth = 0.7f;
        float batteryHeight = 0.25f;
        float batteryX = -0.35f;
        float batteryY = -0.25f;
        drawColorRect(batteryX, batteryY, batteryWidth, 0.02f, 1.0f, 1.0f, 1.0f);
        drawColorRect(batteryX, batteryY + batteryHeight, batteryWidth, 0.02f, 1.0f, 1.0f, 1.0f);
        drawColorRect(batteryX, batteryY, 0.02f, batteryHeight, 1.0f, 1.0f, 1.0f);
        drawColorRect(batteryX + batteryWidth, batteryY, 0.02f, batteryHeight, 1.0f, 1.0f, 1.0f);
        float fillWidth = (batteryWidth - 0.08f) * (battery / 100.0f);
        float fillX = batteryX + batteryWidth - 0.04f - fillWidth;
        float r = battery > 20 ? 0.0f : (battery > 10 ? 1.0f : 1.0f);
        float g = battery > 20 ? 1.0f : (battery > 10 ? 1.0f : 0.0f);
        float b = battery > 20 ? 0.0f : (battery > 10 ? 0.0f : 0.0f);
        drawColorRect(fillX, batteryY + 0.04f, fillWidth, batteryHeight - 0.08f, r, g, b);
        if (arrowLeftTex) drawRect(-0.5f, -0.35f, 0.15f, 0.15f, arrowLeftTex);
    }
    if (watchFrameTex) drawRect(-0.75f, -0.75f, 1.5f, 1.5f, watchFrameTex);
}

void Watch2D_Init() {
    for (int i = 0; i < 10; i++) digitTextures[i] = 0;
    arrowRightTex = arrowLeftTex = nameTex = ekgTex = warningTex = percentTex = bpmLabelTex = grassTex = watchFrameTex = 0;

    shaderProgram = createShader("watch2d.vert", "watch2d.frag");
    colorShaderProgram = createShader("watch2d_color.vert", "watch2d_color.frag");

    glGenVertexArrays(1, &VAOrect);
    glGenBuffers(1, &VBOrect);
    glBindVertexArray(VAOrect);
    glBindBuffer(GL_ARRAY_BUFFER, VBOrect);
    glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glGenVertexArrays(1, &VAOcolor);
    glGenBuffers(1, &VBOcolor);
    glBindVertexArray(VAOcolor);
    glBindBuffer(GL_ARRAY_BUFFER, VBOcolor);
    glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    char path[64];
    for (int i = 0; i < 10; i++) {
        snprintf(path, sizeof(path), "digit%d.png", i);
        loadTex(digitTextures[i], path);
    }
    loadTex(arrowRightTex, "arrow_right.png");
    loadTex(arrowLeftTex, "arrow_left.png");
    loadTex(nameTex, "name.png");
    loadTex(ekgTex, "ekg.png", true);
    loadTex(warningTex, "warning.png");
    loadTex(percentTex, "percent.png");
    loadTex(bpmLabelTex, "bpm_label.png");
    loadTex(grassTex, "grass.jpeg");
    loadTex(watchFrameTex, "watch_frame.png");
}

void Watch2D_Draw(int screen, int hours, int minutes, int seconds, int bpm, bool showWarning, int battery,
    double ekgOffset, float ekgCompressionFactor)
{
    if (screen == 0)
        drawClockScreen(hours, minutes, seconds, battery);
    else if (screen == 1)
        drawHeartScreen(bpm, battery, ekgOffset, ekgCompressionFactor, showWarning);
    else
        drawBatteryScreen(battery);
}
