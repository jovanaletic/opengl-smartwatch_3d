#pragma once
/* WatchUI – ekran sata u FBO (vreme, BPM, baterija), klik na strelice menja ekran. */

#include <GL/glew.h>
#include <GLFW/glfw3.h>

enum WatchScreen { SCREEN_TIME = 0, SCREEN_HEART = 1, SCREEN_BATTERY = 2 };

/* Inicijalizacija Watch UI: FBO, teksture sata. Poziva se posle Scene::init(). */
void WatchUI_Init(unsigned int shaderScreen, unsigned int VAO_quad, unsigned int VBO_quad,
    unsigned int screenWidth, unsigned int screenHeight);

/* Postavi vreme sata (npr. iz sistemskog vremena na početku). */
void WatchUI_SetTime(int hours, int minutes, int seconds);

/* Ažuriranje sat/BPM/baterija/EKG. */
void WatchUI_Update(double dt, bool keyDPressed);

/* Crtanje sadržaja sata u FBO. Na kraju postavlja viewport na screenWidth x screenHeight. */
void WatchUI_RenderToFBO(double dt, unsigned int screenWidth, unsigned int screenHeight);

/* Klik na strelice (menja currentScreen). Poziva se samo kad je watchInFront true. */
void WatchUI_CheckArrowClick(GLFWwindow* window, unsigned int screenWidth, unsigned int screenHeight, bool watchInFront);

/* Pristup stanju za glavnu petlju (npr. running = keyDPressed && getCurrentScreen() == SCREEN_HEART). */
WatchScreen WatchUI_GetCurrentScreen(void);

/* Tekstura FBO sata i okvir (za Scene crtanje 3D ekrana i overlay). */
unsigned int WatchUI_GetTexWatchFBO(void);
unsigned int WatchUI_GetTexWatchFrame(void);
unsigned int WatchUI_GetFboWidth(void);
unsigned int WatchUI_GetFboHeight(void);

/* Oslobodi resurse (FBO, teksture sata). */
void WatchUI_Shutdown(void);
