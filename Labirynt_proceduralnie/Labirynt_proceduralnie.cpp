#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <windows.h>
#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <vector>
#include <stack>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <random>
#include <cmath>
#include <string>

using namespace std;

//Struktura do zarządzania teksturami
struct TextureManager {
    GLuint wallTexture;      // tekstura ścian 
    GLuint floorTexture;     // tekstura podłogi
    GLuint ceilingTexture;   // tekstura sufitu


    GLuint loadTexture(const char* filename) {
        int width, height, channels;
        unsigned char* data = stbi_load(filename, &width, &height, &channels, 0);

        if (!data) {
            printf("Nie mozna zaladowac tekstury: %s\n", filename);
            return 0;
        }

        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        stbi_image_free(data);
        return textureID;
    }
    // Próbuje załadować wszystkie tekstury, w razie braku plików tworzy zastępcze
    bool loadAllTextures() {
        // Załaduj teksturę ścian
        wallTexture = loadTexture("wall.jpg");
        if (!wallTexture) {
            // Jeśli nie ma pliku, utwórz prostą teksturę 
            wallTexture = createTexture(0.55f, 0.50f, 0.45f);
        }

        // Załaduj teksturę podłogi 
        floorTexture = loadTexture("kamien.jpg");
        if (!floorTexture) {
            floorTexture = createTexture(0.35f, 0.30f, 0.25f);
        }

        // Załaduj teksturę sufitu
        ceilingTexture = loadTexture("wall.jpg");
        if (!ceilingTexture) {
            ceilingTexture = createTexture(0.20f, 0.20f, 0.22f);
        }

 

        return true;
    }

    // Tworzy prostą teksturę w kolorze
    GLuint createTexture(float r, float g, float b) {
        unsigned char data[64 * 64 * 3];
        for (int i = 0; i < 64 * 64; i++) {
            data[i * 3] = (unsigned char)(r * 255);
            data[i * 3 + 1] = (unsigned char)(g * 255);
            data[i * 3 + 2] = (unsigned char)(b * 255);
        }

        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 64, 64, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        return textureID;
    }
};


// Labirynt 

// Kierunki: góra, dół, lewo, prawo (używane do poruszania się po siatce)
int dx[] = { 0, 0, -1, 1 };
int dy[] = { -1, 1, 0, 0 };

// Struktura reprezentująca pojedynczą komórkę labiryntu
struct Cell { int x, y; Cell(int _x, int _y) : x(_x), y(_y) {} };

class Maze {
public:
    int width, height;                     // Rozmiary labiryntu
    vector<vector<int>> grid;              // 0 = ściana, 1 = ścieżka
    pair<int, int> startPos;               // Punkt startowy
    pair<int, int> endPos;                 // Punkt końcowy (meta)

    Maze(int w, int h) : width(w), height(h) {
        grid = vector<vector<int>>(height, vector<int>(width, 0));
        startPos = { 1, 1 };               // Zaczynamy od (1,1) 
        endPos = { width - 2, height - 2 }; // Meta przy przeciwległej krawędzi
    }

    // Sprawdza czy dana komórka jest w granicach i czy jest ścianą (dostępna do wykopania)
    bool isValid(int x, int y) {
        if (x <= 0 || x >= width - 1 || y <= 0 || y >= height - 1) return false;
        return grid[y][x] == 0; // 0 oznacza ścianę, czyli jeszcze nie odwiedzona
    }
    // Algorym generowania labiryntu (BACKTRACKING)
    void generateMaze() {
        random_device rd;
        mt19937 g(rd());
        // Wyzeruj cały labirynt (wszystkie komórki to ściany)
        for (int y = 0; y < height; ++y)
            fill(grid[y].begin(), grid[y].end(), 0);

        stack<Cell> s; // Stos do backtrackingu
        s.push(Cell(startPos.first, startPos.second));
        grid[startPos.second][startPos.first] = 1; // Oznacz start jako ścieżkę

        while (!s.empty()) {
            Cell current = s.top();
            // Losowa kolejność kierunków (0,1,2,3)
            vector<int> dirs = { 0, 1, 2, 3 };
            shuffle(dirs.begin(), dirs.end(), g);
            bool moved = false;
            // Sprawdź wszystkie 4 kierunki
            for (int i = 0; i < 4; i++) {
                // Przeskocz o 2 komórki w danym kierunku (dx/dy * 2)
                int nx = current.x + dx[dirs[i]] * 2;
                int ny = current.y + dy[dirs[i]] * 2;
                if (isValid(nx, ny)) { // Czy trafiliśmy na nieodwiedzoną ścianę?
                    // "Wykop" przejście: środkowa komórka między obecną a nową
                    grid[current.y + dy[dirs[i]]][current.x + dx[dirs[i]]] = 1;
                    // Nowa komórka (oddalona o 2) staje się ścieżką
                    grid[ny][nx] = 1;
                    // Wrzuć nową komórkę na stos i kontynuuj
                    s.push(Cell(nx, ny));
                    moved = true;
                    break;
                }
            }
            if (!moved) s.pop(); // Ślepy zaułek - wróć do poprzedniej komórki
        }
        // Upewnij się że start i meta są ścieżkami (na wszelki wypadek)
        grid[startPos.second][startPos.first] = 1;
        grid[endPos.second][endPos.first] = 1;
    }
};

// Globalne zmienne
Maze maze(21, 21);
TextureManager textures;

float playerX, playerZ;
float yaw = 0.0f;
float pitch = 0.0f;
const float PLAYER_HEIGHT = 0.5f;
const float CELL_SIZE = 1.0f;
const float WALL_HEIGHT = 1.2f;
const float MOVE_SPEED = 3.0f;
const float MOUSE_SENS = 0.08f;

double lastMouseX = 0, lastMouseY = 0;
bool firstMouse = true;

GLFWwindow* window;
float deltaTime = 0.0f, lastFrame = 0.0f;

// Sprawdzanie kolizji 
// Sprawdza czy dany punkt (wx, wz) znajduje się w ścianie
bool isWall(float wx, float wz) {
    int cx = (int)(wx / CELL_SIZE);
    int cz = (int)(wz / CELL_SIZE);
    if (cx < 0 || cx >= maze.width || cz < 0 || cz >= maze.height) return true;
    return maze.grid[cz][cx] == 0;   // 0 = ściana
}
// Sprawdza czy gracz może przejść na nową pozycję 
bool canMove(float nx, float nz) {
    float margin = 0.25f;
    return !isWall(nx - margin, nz - margin) &&
        !isWall(nx + margin, nz - margin) &&
        !isWall(nx - margin, nz + margin) &&
        !isWall(nx + margin, nz + margin);
}

// Rysuje pojedynczy teksturowany czworokąt (ściana/podłoga/sufit)
void drawTexturedQuad(float x1, float y1, float z1,
    float x2, float y2, float z2,
    float x3, float y3, float z3,
    float x4, float y4, float z4,
    float tx1, float ty1, float tx2, float ty2,
    float tx3, float ty3, float tx4, float ty4) {
    glBegin(GL_QUADS);
    glTexCoord2f(tx1, ty1); glVertex3f(x1, y1, z1);
    glTexCoord2f(tx2, ty2); glVertex3f(x2, y2, z2);
    glTexCoord2f(tx3, ty3); glVertex3f(x3, y3, z3);
    glTexCoord2f(tx4, ty4); glVertex3f(x4, y4, z4);
    glEnd();
}
// Rysuje podłogę i sufit dla wszystkich komórek będących ścieżkami
void drawFloor() {
    glBindTexture(GL_TEXTURE_2D, textures.floorTexture);
    glEnable(GL_TEXTURE_2D);

    for (int z = 0; z < maze.height; z++) {
        for (int x = 0; x < maze.width; x++) {
            if (maze.grid[z][x] == 1) {
                float fx = x * CELL_SIZE;
                float fz = z * CELL_SIZE;

                // Podłoga z teksturą
                glNormal3f(0, 1, 0);
                drawTexturedQuad(fx, 0, fz,
                    fx + CELL_SIZE, 0, fz,
                    fx + CELL_SIZE, 0, fz + CELL_SIZE,
                    fx, 0, fz + CELL_SIZE,
                    0, 0, 1, 0, 1, 1, 0, 1);

                // Sufit z teksturą
                glBindTexture(GL_TEXTURE_2D, textures.ceilingTexture);
                glNormal3f(0, -1, 0);
                drawTexturedQuad(fx, WALL_HEIGHT, fz,
                    fx, WALL_HEIGHT, fz + CELL_SIZE,
                    fx + CELL_SIZE, WALL_HEIGHT, fz + CELL_SIZE,
                    fx + CELL_SIZE, WALL_HEIGHT, fz,
                    0, 0, 0, 1, 1, 1, 1, 0);

                glBindTexture(GL_TEXTURE_2D, textures.floorTexture);
            }
        }
    }

    glDisable(GL_TEXTURE_2D);
}
// Rysuje wszystkie ściany (dla komórek gdzie grid == 0)
void drawWalls() {
    glBindTexture(GL_TEXTURE_2D, textures.wallTexture);
    glEnable(GL_TEXTURE_2D);

    for (int z = 0; z < maze.height; z++) {
        for (int x = 0; x < maze.width; x++) {
            if (maze.grid[z][x] == 0) { // Tylko dla ścian
                float fx = x * CELL_SIZE;
                float fz = z * CELL_SIZE;

                
                float texRepeatX = 1.0f;
                float texRepeatY = WALL_HEIGHT;

                // Przód
                glNormal3f(0, 0, -1);
                drawTexturedQuad(fx, 0, fz,
                    fx + CELL_SIZE, 0, fz,
                    fx + CELL_SIZE, WALL_HEIGHT, fz,
                    fx, WALL_HEIGHT, fz,
                    0, 0, texRepeatX, 0, texRepeatX, texRepeatY, 0, texRepeatY);

                // Tył
                glNormal3f(0, 0, 1);
                drawTexturedQuad(fx, 0, fz + CELL_SIZE,
                    fx, WALL_HEIGHT, fz + CELL_SIZE,
                    fx + CELL_SIZE, WALL_HEIGHT, fz + CELL_SIZE,
                    fx + CELL_SIZE, 0, fz + CELL_SIZE,
                    0, 0, 0, texRepeatY, texRepeatX, texRepeatY, texRepeatX, 0);

                // Lewa
                glNormal3f(-1, 0, 0);
                drawTexturedQuad(fx, 0, fz,
                    fx, WALL_HEIGHT, fz,
                    fx, WALL_HEIGHT, fz + CELL_SIZE,
                    fx, 0, fz + CELL_SIZE,
                    0, 0, 0, texRepeatY, texRepeatX, texRepeatY, texRepeatX, 0);

                // Prawa
                glNormal3f(1, 0, 0);
                drawTexturedQuad(fx + CELL_SIZE, 0, fz,
                    fx + CELL_SIZE, 0, fz + CELL_SIZE,
                    fx + CELL_SIZE, WALL_HEIGHT, fz + CELL_SIZE,
                    fx + CELL_SIZE, WALL_HEIGHT, fz,
                    0, 0, texRepeatX, 0, texRepeatX, texRepeatY, 0, texRepeatY);
            }
        }
    }

    glDisable(GL_TEXTURE_2D);
}

// Rysuje znacznik mety (czerwony słup)
void drawGoal() {
    float gx = maze.endPos.first * CELL_SIZE + CELL_SIZE * 0.5f;
    float gz = maze.endPos.second * CELL_SIZE + CELL_SIZE * 0.5f;
    float t = (float)glfwGetTime();
    float pulse = 0.7f + 0.3f * sinf(t * 2.5f); // Efekt pulsowania

    glDisable(GL_TEXTURE_2D);
    glColor3f(1.0f * pulse, 0.2f, 0.2f * pulse);
    glBegin(GL_QUADS);
    float s = 0.15f;
    glVertex3f(gx - s, 0, gz - s); glVertex3f(gx + s, 0, gz - s);
    glVertex3f(gx + s, WALL_HEIGHT, gz - s); glVertex3f(gx - s, WALL_HEIGHT, gz - s);

    glVertex3f(gx - s, 0, gz + s); glVertex3f(gx - s, WALL_HEIGHT, gz + s);
    glVertex3f(gx + s, WALL_HEIGHT, gz + s); glVertex3f(gx + s, 0, gz + s);

    glVertex3f(gx - s, 0, gz - s); glVertex3f(gx - s, 0, gz + s);
    glVertex3f(gx - s, WALL_HEIGHT, gz + s); glVertex3f(gx - s, WALL_HEIGHT, gz - s);

    glVertex3f(gx + s, 0, gz - s); glVertex3f(gx + s, WALL_HEIGHT, gz - s);
    glVertex3f(gx + s, WALL_HEIGHT, gz + s); glVertex3f(gx + s, 0, gz + s);
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

// Rysuje minimapę w prawym górnym rogu(widok z góry, 2D)
void drawMinimap() {
    int W, H;
    glfwGetFramebufferSize(window, &W, &H);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, W, H, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    int mapOffX = W - maze.width * 4 - 8;  // Pozycja X minimapy
    int mapOffY = 8;                       // Pozycja Y minimapy
    int cs = 4;                            // Rozmiar jednej komórki w pikselach

    // Rysuj labirynt (ściany = ciemne, ścieżki = jasne)
    for (int z = 0; z < maze.height; z++) {
        for (int x = 0; x < maze.width; x++) {
            if (maze.grid[z][x] == 1)
                glColor3f(0.7f, 0.7f, 0.6f);
            else
                glColor3f(0.1f, 0.1f, 0.1f);
            glBegin(GL_QUADS);
            glVertex2i(mapOffX + x * cs, mapOffY + z * cs);
            glVertex2i(mapOffX + x * cs + cs, mapOffY + z * cs);
            glVertex2i(mapOffX + x * cs + cs, mapOffY + z * cs + cs);
            glVertex2i(mapOffX + x * cs, mapOffY + z * cs + cs);
            glEnd();
        }
    }
    // Rysuj gracza (zielony kwadrat)
    int px = (int)(playerX / CELL_SIZE);
    int pz = (int)(playerZ / CELL_SIZE);
    glColor3f(0.0f, 1.0f, 0.3f);
    glBegin(GL_QUADS);
    glVertex2i(mapOffX + px * cs, mapOffY + pz * cs);
    glVertex2i(mapOffX + px * cs + cs, mapOffY + pz * cs);
    glVertex2i(mapOffX + px * cs + cs, mapOffY + pz * cs + cs);
    glVertex2i(mapOffX + px * cs, mapOffY + pz * cs + cs);
    glEnd();
    // Rysuj metę (czerwony kwadrat)
    glColor3f(1.0f, 0.2f, 0.2f);
    int ex = maze.endPos.first, ez = maze.endPos.second;
    glBegin(GL_QUADS);
    glVertex2i(mapOffX + ex * cs, mapOffY + ez * cs);
    glVertex2i(mapOffX + ex * cs + cs, mapOffY + ez * cs);
    glVertex2i(mapOffX + ex * cs + cs, mapOffY + ez * cs + cs);
    glVertex2i(mapOffX + ex * cs, mapOffY + ez * cs + cs);
    glEnd();
    // Rysuj celownik na środku ekranu
    int cx = W / 2, cy = H / 2;
    glColor3f(1, 1, 1);
    glLineWidth(1.5f);
    glBegin(GL_LINES);
    glVertex2i(cx - 8, cy); glVertex2i(cx + 8, cy);
    glVertex2i(cx, cy - 8); glVertex2i(cx, cy + 8);
    glEnd();

    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}
// Obsługa myszy - obracanie kamery
void mouseCallback(GLFWwindow* w, double xpos, double ypos) {
    if (firstMouse) {
        lastMouseX = xpos; lastMouseY = ypos;
        firstMouse = false;
    }
    float dx_ = (float)(xpos - lastMouseX) * MOUSE_SENS;
    float dy_ = (float)(ypos - lastMouseY) * MOUSE_SENS;
    lastMouseX = xpos; lastMouseY = ypos;

    yaw -= dx_;
    pitch -= dy_;
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
}
// Obsługa klawiatury - poruszanie się postacią
void processInput() {
    float rad = yaw * 3.14159f / 180.0f;
    float fwdX = sinf(rad);
    float fwdZ = cosf(rad);
    float sideX = cosf(rad);
    float sideZ = -sinf(rad);
    float speed = MOVE_SPEED * deltaTime;
    float nx = playerX, nz = playerZ;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        nx += fwdX * speed; nz += fwdZ * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        nx -= fwdX * speed; nz -= fwdZ * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        nx += sideX * speed; nz += sideZ * speed;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        nx -= sideX * speed; nz -= sideZ * speed;
    }

    if (canMove(nx, playerZ)) playerX = nx;
    if (canMove(playerX, nz)) playerZ = nz;
}
// Obsługa specjalnych klawiszy (ESC, R)
void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE)
            glfwSetWindowShouldClose(w, true);
        if (key == GLFW_KEY_R) {
            maze.generateMaze();
            playerX = (maze.startPos.first + 0.5f) * CELL_SIZE;
            playerZ = (maze.startPos.second + 0.5f) * CELL_SIZE;
            yaw = 0; pitch = 0;
        }
    }
}

// Konfiguracja oświetlenia
void setupLighting() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    GLfloat ambientLight[] = { 0.3f, 0.28f, 0.25f, 1.0f };
    GLfloat diffuseLight[] = { 0.9f, 0.85f, 0.75f, 1.0f };
    GLfloat specularLight[] = { 0.4f, 0.4f, 0.4f, 1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specularLight);

    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 0.5f);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.2f);
    glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.1f);
}
// Aktualizuje pozycję światła 
void updateLight() {
    GLfloat pos[] = { playerX, PLAYER_HEIGHT + 0.5f, playerZ, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, pos);
}


int main() {
    srand((unsigned)time(0));
    maze.generateMaze();

    if (!glfwInit()) return -1;

    window = glfwCreateWindow(1024, 768, "Labirynt 3D - generowany proceduralnie", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);

    // Załaduj wszystkie tekstury
    textures.loadAllTextures();

    // Mgła
    glEnable(GL_FOG);
    GLfloat fogColor[] = { 0.1f, 0.08f, 0.05f, 1.0f };
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 2.0f);
    glFogf(GL_FOG_END, 5.0f);

    setupLighting();

    // Ustaw początkową pozycję gracza
    playerX = (maze.startPos.first + 0.5f) * CELL_SIZE;
    playerZ = (maze.startPos.second + 0.5f) * CELL_SIZE;
    // Główna pętla gry
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(); // Obsługa ruchu

        int W, H;
        glfwGetFramebufferSize(window, &W, &H);
        glViewport(0, 0, W, H);

        glClearColor(0.1f, 0.08f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        // Ustaw perspektywę
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(75.0, (double)W / H, 0.05, 50.0);
        // Ustaw kamerę (pozycja gracza + kierunek patrzenia)
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        float radYaw = yaw * 3.14159f / 180.0f;
        float radPitch = pitch * 3.14159f / 180.0f;
        float lookX = playerX + sinf(radYaw) * cosf(radPitch);
        float lookY = PLAYER_HEIGHT + sinf(radPitch);
        float lookZ = playerZ + cosf(radYaw) * cosf(radPitch);
        gluLookAt(playerX, PLAYER_HEIGHT, playerZ,
            lookX, lookY, lookZ,
            0.0, 1.0, 0.0);

        updateLight();
        // Rysuj scenę
        drawFloor();
        drawWalls();
        drawGoal();
        // Minimapa (bez oświetlenia i mgły)
        glDisable(GL_LIGHTING);
        glDisable(GL_FOG);
        drawMinimap();
        glEnable(GL_LIGHTING);
        glEnable(GL_FOG);

        // Sprawdź czy gracz dotarł do mety
        float dx2 = playerX - (maze.endPos.first + 0.5f) * CELL_SIZE;
        float dz2 = playerZ - (maze.endPos.second + 0.5f) * CELL_SIZE;
        if (sqrtf(dx2 * dx2 + dz2 * dz2) < 0.6f) {
            maze.generateMaze();
            playerX = (maze.startPos.first + 0.5f) * CELL_SIZE;
            playerZ = (maze.startPos.second + 0.5f) * CELL_SIZE;
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}