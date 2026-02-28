#define _CRT_SECURE_NO_WARNINGS
#include "Util.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Autor: Nedeljko Tesanovic
// Opis: pomocne funkcije za ucitavanje sejdera i tekstura

/* Cita sejder iz fajla, kompajlira ga i vraca OpenGL handle (vertex ili fragment). */
unsigned int compileShader(GLenum type, const char* source)
{
    //Uzima kod u fajlu na putanji "source", kompajlira ga i vraca sejder tipa "type"
    //Citanje izvornog koda iz fajla
    std::string content = "";
    std::ifstream file(source);
    std::stringstream ss;
    if (file.is_open())
    {
        ss << file.rdbuf();
        file.close();
        std::cout << "Uspjesno procitao fajl sa putanje \"" << source << "\"!" << std::endl;
    }
    else {
        ss << "";
        std::cout << "Greska pri citanju fajla sa putanje \"" << source << "\"!" << std::endl;
    }
    std::string temp = ss.str();
    const char* sourceCode = temp.c_str(); //Izvorni kod sejdera koji citamo iz fajla na putanji "source"

    int shader = glCreateShader(type); //Napravimo prazan sejder odredjenog tipa (vertex ili fragment)

    int success; //Da li je kompajliranje bilo uspjesno (1 - da)
    char infoLog[512]; //Poruka o gresci (Objasnjava sta je puklo unutar sejdera)
    glShaderSource(shader, 1, &sourceCode, NULL); //Postavi izvorni kod sejdera
    glCompileShader(shader); //Kompajliraj sejder

    glGetShaderiv(shader, GL_COMPILE_STATUS, &success); //Provjeri da li je sejder uspjesno kompajliran
    if (success == GL_FALSE)
    {
        glGetShaderInfoLog(shader, 512, NULL, infoLog); //Pribavi poruku o gresci
        if (type == GL_VERTEX_SHADER)
            printf("VERTEX");
        else if (type == GL_FRAGMENT_SHADER)
            printf("FRAGMENT");
        printf(" sejder ima gresku! Greska: \n");
        printf(infoLog);
    }
    return shader;
}

/* Pravljenje celog sejder programa od vertex + fragment fajlova. */
unsigned int createShader(const char* vsSource, const char* fsSource)
{
    //Pravi objedinjeni sejder program koji se sastoji od Vertex sejdera ciji je kod na putanji vsSource

    unsigned int program; //Objedinjeni sejder
    unsigned int vertexShader; //Verteks sejder (za prostorne podatke)
    unsigned int fragmentShader; //Fragment sejder (za boje, teksture itd)

    program = glCreateProgram(); //Napravi prazan objedinjeni sejder program

    vertexShader = compileShader(GL_VERTEX_SHADER, vsSource); //Napravi i kompajliraj vertex sejder
    fragmentShader = compileShader(GL_FRAGMENT_SHADER, fsSource); //Napravi i kompajliraj fragment sejder

    //Zakaci verteks i fragment sejdere za objedinjeni program
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);

    glLinkProgram(program); //Povezi ih u jedan objedinjeni sejder program
    glValidateProgram(program); //Izvrsi provjeru novopecenog programa

    int success;
    char infoLog[512];
    glGetProgramiv(program, GL_VALIDATE_STATUS, &success); //Slicno kao za sejdere
    if (success == GL_FALSE)
    {
        glGetShaderInfoLog(program, 512, NULL, infoLog);
        std::cout << "Objedinjeni sejder ima gresku! Greska: \n";
        std::cout << infoLog << std::endl;
    }

    //Posto su kodovi sejdera u objedinjenom sejderu, oni pojedinacni programi nam ne trebaju, pa ih brisemo zarad ustede na memoriji
    glDetachShader(program, vertexShader);
    glDeleteShader(vertexShader);
    glDetachShader(program, fragmentShader);
    glDeleteShader(fragmentShader);

    return program;
}

/* Ucitava sliku (PNG, JPG itd.) u OpenGL 2D teksturu; vertikalno je okrenuta za ispravan prikaz. */
unsigned loadImageToTexture(const char* filePath) {
    int TextureWidth;
    int TextureHeight;
    int TextureChannels;
    unsigned char* ImageData =
        stbi_load(filePath, &TextureWidth, &TextureHeight, &TextureChannels, STBI_rgb_alpha);

    TextureChannels = 4;
    if (ImageData != NULL)
    {
        //Slike se osnovno ucitavaju naopako pa se moraju ispraviti da budu uspravne
        stbi__vertical_flip(ImageData, TextureWidth, TextureHeight, TextureChannels);

        // Provjerava koji je format boja ucitane slike
        GLint InternalFormat = -1;
        switch (TextureChannels) {
        case 1: InternalFormat = GL_RED; break;
        case 2: InternalFormat = GL_RG; break;
        case 3: InternalFormat = GL_RGB; break;
        case 4: InternalFormat = GL_RGBA; break;
        default: InternalFormat = GL_RGB; break;
        }

        unsigned int Texture;
        glGenTextures(1, &Texture);
        glBindTexture(GL_TEXTURE_2D, Texture);
        glTexImage2D(GL_TEXTURE_2D, 0, InternalFormat, TextureWidth, TextureHeight, 0, InternalFormat, GL_UNSIGNED_BYTE, ImageData);
        glBindTexture(GL_TEXTURE_2D, 0);
        // oslobadjanje memorije zauzete sa stbi_load posto vise nije potrebna
        stbi_image_free(ImageData);
        return Texture;
    }
    else
    {
        std::cout << "Textura nije ucitana! Putanja texture: " << filePath << std::endl;
        stbi_image_free(ImageData);
        return 0;
    }
}

/* Ucitava sliku i pravi GLFW kursor (npr. heart.png za kursor u obliku srca). */
GLFWcursor* loadImageToCursor(const char* filePath) {
    int TextureWidth;
    int TextureHeight;
    int TextureChannels;

    unsigned char* ImageData = stbi_load(filePath, &TextureWidth, &TextureHeight, &TextureChannels, 0);

    if (ImageData != NULL)
    {
        GLFWimage image;
        image.width = TextureWidth;
        image.height = TextureHeight;
        image.pixels = ImageData;

        // Tacka na povr?ini slike kursora koja se pona?a kao hitboks
        int hotspotX = image.width / 6;
        int hotspotY = image.height / 6;

        GLFWcursor* cursor = glfwCreateCursor(&image, hotspotX, hotspotY);
        stbi_image_free(ImageData);
        return cursor;
    }
    else {
        std::cout << "Kursor nije ucitan! Putanja kursora: " << filePath << std::endl;
        stbi_image_free(ImageData);
        return nullptr;
    }
}

/* Kursor u obliku srca: prvo res/heart.png, ako nema – proceduralno nacrtano srce. */
GLFWcursor* createHeartCursor() {
    GLFWcursor* cur = loadImageToCursor("res/heart.png");
    if (cur) return cur;
    const int W = 32, H = 32;
    unsigned char* pixels = (unsigned char*)malloc(W * H * 4);
    if (!pixels) return nullptr;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float nx = (x - W * 0.5f) / (W * 0.5f);
            float ny = -(y - H * 0.5f) / (H * 0.5f);
            float heart = (nx * nx + ny * ny - 1) * (nx * nx + ny * ny - 1) * (nx * nx + ny * ny - 1) - nx * nx * ny * ny * ny;
            int i = (y * W + x) * 4;
            pixels[i] = 255;
            pixels[i + 1] = 0;
            pixels[i + 2] = 50;
            pixels[i + 3] = heart > 0 ? 0 : 255;
        }
    }
    GLFWimage img;
    img.width = W;
    img.height = H;
    img.pixels = pixels;
    cur = glfwCreateCursor(&img, W / 2, H / 2);
    free(pixels);
    return cur;
}

/* Parsira jedan indeks iz OBJ face linije (v/vt/vn ili v//vn itd.). */
static void parseFaceVertex(const char* s, int* v, int* vt, int* vn) {
    *v = *vt = *vn = 0;
    if (sscanf(s, "%d/%d/%d", v, vt, vn) == 3) return;
    if (sscanf(s, "%d//%d", v, vn) == 2) return;
    if (sscanf(s, "%d/%d", v, vt) == 2) return;
    sscanf(s, "%d", v);
}

/* Jedan vertex u VAO formatu: pozicija, boja, UV(0,0), normala (iz OBJ indeksa). */
static void pushObjVert(int i, int n, const int* iv, const int* in,
    const std::vector<float>& pos, const std::vector<float>& nrm, std::vector<float>& outVerts) {
    if (i < 0 || i >= n || iv[i] < 1 || iv[i] > (int)(pos.size() / 3)) return;
    int idx = (iv[i] - 1) * 3;
    outVerts.push_back(pos[idx]); outVerts.push_back(pos[idx+1]); outVerts.push_back(pos[idx+2]);
    outVerts.push_back(1.f); outVerts.push_back(1.f); outVerts.push_back(1.f); outVerts.push_back(1.f);
    outVerts.push_back(0.f); outVerts.push_back(0.f);
    if (in[i] >= 1 && in[i] <= (int)(nrm.size() / 3)) {
        int nidx = (in[i] - 1) * 3;
        outVerts.push_back(nrm[nidx]); outVerts.push_back(nrm[nidx+1]); outVerts.push_back(nrm[nidx+2]);
    } else {
        outVerts.push_back(0.f); outVerts.push_back(1.f); outVerts.push_back(0.f);
    }
}

/* Ucitava OBJ fajl (v, vn, f), pravi VAO sa pozicijom/bojom/UV/normalom; outCount = broj indeksa za glDrawArrays. */
unsigned int loadOBJToVAO(const char* filePath, int* outCount) {
    std::ifstream f(filePath);
    if (!f.is_open()) { if (outCount) *outCount = 0; return 0; }
    std::vector<float> pos, nrm;
    std::vector<float> outVerts;
    pos.reserve(1024 * 3);
    nrm.reserve(1024 * 3);
    std::string line;
    while (std::getline(f, line)) {
        if (line.size() < 2) continue;
        if (line[0] == 'v' && line[1] == ' ') {
            float x, y, z;
            if (sscanf(line.c_str() + 2, "%f %f %f", &x, &y, &z) == 3) {
                pos.push_back(x); pos.push_back(y); pos.push_back(z);
            }
        } else if (line[0] == 'v' && line[1] == 'n' && line[2] == ' ') {
            float x, y, z;
            if (sscanf(line.c_str() + 3, "%f %f %f", &x, &y, &z) == 3) {
                nrm.push_back(x); nrm.push_back(y); nrm.push_back(z);
            }
        } else if (line[0] == 'f' && line[1] == ' ') {
            const char* p = line.c_str() + 2;
            int iv[4], it[4], in[4];
            int n = 0;
            while (*p && n < 4) {
                while (*p == ' ') p++;
                if (!*p) break;
                parseFaceVertex(p, &iv[n], &it[n], &in[n]);
                while (*p && *p != ' ') p++;
                n++;
            }
            if (n == 3) {
                pushObjVert(0, n, iv, in, pos, nrm, outVerts);
                pushObjVert(1, n, iv, in, pos, nrm, outVerts);
                pushObjVert(2, n, iv, in, pos, nrm, outVerts);
            } else if (n == 4) {
                pushObjVert(0, n, iv, in, pos, nrm, outVerts);
                pushObjVert(1, n, iv, in, pos, nrm, outVerts);
                pushObjVert(2, n, iv, in, pos, nrm, outVerts);
                pushObjVert(0, n, iv, in, pos, nrm, outVerts);
                pushObjVert(2, n, iv, in, pos, nrm, outVerts);
                pushObjVert(3, n, iv, in, pos, nrm, outVerts);
            }
        }
    }
    f.close();
    if (outVerts.empty()) { if (outCount) *outCount = 0; return 0; }
    const unsigned int stride = 12 * sizeof(float);
    const int numVerts = (int)(outVerts.size() / 12);
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(outVerts.size() * sizeof(float)), outVerts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(7 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(9 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    if (outCount) *outCount = numVerts;
    return VAO;
}