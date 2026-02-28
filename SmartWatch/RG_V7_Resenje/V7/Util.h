#pragma once
/* Util – ucitavanje sejdera, tekstura, kursora i OBJ modela. */

#include <GL/glew.h>
#include <GLFW/glfw3.h>

unsigned int createShader(const char* vsSource, const char* fsSource);
unsigned loadImageToTexture(const char* filePath);
GLFWcursor* loadImageToCursor(const char* filePath);
GLFWcursor* createHeartCursor();

/* Ucitaj OBJ model (v, vn, f). Vraca VAO; u *outCount broj indeksa za glDrawArrays. 0 ako ne uspe. */
unsigned int loadOBJToVAO(const char* filePath, int* outCount);