#pragma once
/* Scene – 3D scena (kamera, pod, zgrade, ruka, sat, senke). */

#include <GL/glew.h>
#include <GLFW/glfw3.h>

/* Inicijalizacija scene: šaderi, VAO-ovi, teksture (podloga, zgrade, koža, sat, nebo, student), ruka. */
void Scene_Init(unsigned int screenWidth, unsigned int screenHeight);

/* Crtanje cele 3D scene + nebo + overlay sata (SPACE) + student overlay.
 * texWatchFBO, texWatchFrame, fboWidth, fboHeight iz WatchUI. */
void Scene_Render(double dt, bool running, float runBlend, double runBobPhase, float groundOffsetZ,
    bool watchInFront, bool depthTestEnabled, bool cullFaceEnabled,
    unsigned int screenWidth, unsigned int screenHeight,
    unsigned int texWatchFBO, unsigned int texWatchFrame, unsigned int fboWidth, unsigned int fboHeight);

/* Callback za miš (kamera). Registruje se sa glfwSetCursorPosCallback(window, Scene_MouseCallback). */
void Scene_MouseCallback(GLFWwindow* window, double xpos, double ypos);

/* Tasteri 1–4 i SPACE */
void Scene_SetCameraLocked(bool locked);
void Scene_SetFirstMouse(bool first);

/* Pristup za WatchUI_Init: šader i quad koji Scene kreira. */
unsigned int Scene_GetShaderScreen(void);
unsigned int Scene_GetVAOQuad(void);
unsigned int Scene_GetVBOQuad(void);

/* Oslobodi VAO-ove, šadere, teksture scene. */
void Scene_Shutdown(void);
