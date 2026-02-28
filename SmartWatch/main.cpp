// 3D Pametni sat - RG projekat (3D specifikacije 2025/2026)
// Jovana Letic - RA74/2022
// Podela: main.cpp (petlja + input), WatchUI (ekran sata u FBO), Scene (3D scena).
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <ctime>
#include <cmath>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "Util.h"
#include "WatchUI.h"
#include "Scene.h"

const double TARGET_FPS = 75.0;
const double FRAME_TIME = 1.0 / TARGET_FPS;


/* Glavna funkcija: inicijalizacija prozora, OpenGL, Scene i WatchUI,
  zatim glavna petlja (input, update, render, frame limiter).
*/

int main(void) {
    srand((unsigned)time(nullptr));
    time_t now = time(0);
    tm ltm = {};
    if (localtime_s(&ltm, &now) == 0)
        WatchUI_SetTime(ltm.tm_hour, ltm.tm_min, ltm.tm_sec);

    /* Inicijalizacija GLFW i kreiranje fullscreen prozora */
    if (!glfwInit()) { std::cout << "GLFW nije ucitan.\n"; return 1; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* mon = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(mon);
    unsigned int screenWidth = (unsigned)mode->width;
    unsigned int screenHeight = (unsigned)mode->height;

    GLFWwindow* window = glfwCreateWindow((int)screenWidth, (int)screenHeight, "3D Pametni sat", mon, NULL);
    if (!window) { glfwTerminate(); return 2; }
    glfwMakeContextCurrent(window);
    glfwFocusWindow(window);

    if (glewInit() != GLEW_OK) { std::cout << "GLEW nije ucitan.\n"; return 3; }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    bool depthTestEnabled = true;
    bool cullFaceEnabled = true;
    if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
    if (cullFaceEnabled) { glEnable(GL_CULL_FACE); glCullFace(GL_BACK); }

    Scene_Init(screenWidth, screenHeight);
    WatchUI_Init(Scene_GetShaderScreen(), Scene_GetVAOQuad(), Scene_GetVBOQuad(), screenWidth, screenHeight);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, Scene_MouseCallback);
    GLFWcursor* heartCur = createHeartCursor();
    if (heartCur) glfwSetCursor(window, heartCur);

    glClearColor(0.68f, 0.85f, 1.0f, 1.0f);

    bool watchInFront = false;
    bool keyDPressed = false;
    double runBobPhase = 0.0;
    float groundOffsetZ = 0.0f;
    const float GROUND_CYCLE_LENGTH = 20.0f * 25.0f;

    /* -------- Glavna petlja -------- */
    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        double dt = now - lastTime;
        lastTime = now;

        /* Escape zatvara prozor */
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GL_TRUE);

        /* Podešavanje: testiranje dubine (1=uključi, 2=isključi) i uklanjanje naličja (3=uključi, 4=isključi)*/
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) { glEnable(GL_DEPTH_TEST); depthTestEnabled = true; }
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) { glDisable(GL_DEPTH_TEST); depthTestEnabled = false; }
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) { glEnable(GL_CULL_FACE); glCullFace(GL_BACK); cullFaceEnabled = true; }
        if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) { glDisable(GL_CULL_FACE); cullFaceEnabled = false; }

        static bool spaceWasDown = false;
        bool spaceDown = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
        if (spaceDown && !spaceWasDown) {
            watchInFront = !watchInFront;
            Scene_SetCameraLocked(watchInFront);
            if (watchInFront)
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            else {
                Scene_SetFirstMouse(true);
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
        }
        spaceWasDown = spaceDown;

        /* D = trčanje (samo na ekranu srca) */
        keyDPressed = (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS);

        WatchUI_Update(dt, keyDPressed);

        /* Trčanje: pomera se put, bobbing kamere/ruke, runBlend za glatki prelaz kad pustiš D */
        bool running = (keyDPressed && WatchUI_GetCurrentScreen() == SCREEN_HEART);
        static bool wasRunning = false;
        /* runBlend*/
        static float runBlend = 0.0f;
        if (running) {
            runBobPhase += dt * 5.0;
            const double TWO_PI = 6.283185307179586;
            if (runBobPhase > TWO_PI) runBobPhase = fmod(runBobPhase, TWO_PI);
            runBlend = (runBlend + (float)(dt * 5.0f) < 1.0f) ? runBlend + (float)(dt * 5.0f) : 1.0f;
            /* Ažuriraj put tek od drugog frejma trčanja da na početku ne nestane podloga/zgrade*/
            if (wasRunning) {
                groundOffsetZ += (float)(dt * 18.0);
                if (groundOffsetZ >= GROUND_CYCLE_LENGTH)
                    groundOffsetZ -= GROUND_CYCLE_LENGTH;
                if (groundOffsetZ < 0.0f)
                    groundOffsetZ += GROUND_CYCLE_LENGTH;
            }
            wasRunning = true;
        } else {
            wasRunning = false;
            runBlend = (runBlend - (float)(dt * 7.0f) > 0.0f) ? runBlend - (float)(dt * 7.0f) : 0.0f;
        }

        static bool leftMouseWasDown = false;
        bool leftMouseDown = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
        if (watchInFront && leftMouseDown && !leftMouseWasDown)
            WatchUI_CheckArrowClick(window, screenWidth, screenHeight, watchInFront);
        leftMouseWasDown = leftMouseDown;

        /* Crta sadržaj sata u FBO (ceo ekran sata), pa 3D scena koristi tu teksturu */
        WatchUI_RenderToFBO(dt, screenWidth, screenHeight);

        Scene_Render(dt, running, runBlend, runBobPhase, groundOffsetZ,
            watchInFront, depthTestEnabled, cullFaceEnabled,
            screenWidth, screenHeight,
            WatchUI_GetTexWatchFBO(), WatchUI_GetTexWatchFrame(),
            WatchUI_GetFboWidth(), WatchUI_GetFboHeight());

        /* Frame limiter: 75 FPS  */
        while (glfwGetTime() - lastTime < FRAME_TIME) {}
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    /* Oslobađanje resursa i zatvaranje. */
    WatchUI_Shutdown();
    Scene_Shutdown();
    glfwTerminate();
    return 0;
}
