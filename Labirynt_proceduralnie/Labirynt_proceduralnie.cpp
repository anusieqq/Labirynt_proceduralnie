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

// ---- Labirynt ----
int dx[] = { 0, 0, -1, 1 };
int dy[] = { -1, 1, 0, 0 };

struct Cell { int x, y; Cell(int _x, int _y) : x(_x), y(_y) {} };

class Maze {
public:
    int width, height;
    vector<vector<int>> grid;
    pair<int, int> startPos;
    pair<int, int> endPos;

    Maze(int w, int h) : width(w), height(h) {
        grid = vector<vector<int>>(height, vector<int>(width, 0));
        startPos = { 1, 1 };
        endPos = { width - 2, height - 2 };
    }

    bool isValid(int x, int y) {
        if (x <= 0 || x >= width - 1 || y <= 0 || y >= height - 1) return false;
        return grid[y][x] == 0;
    }

    void generateMaze() {
        random_device rd;
        mt19937 g(rd());
        for (int y = 0; y < height; ++y)
            fill(grid[y].begin(), grid[y].end(), 0);

        stack<Cell> s;
        s.push(Cell(startPos.first, startPos.second));
        grid[startPos.second][startPos.first] = 1;

        while (!s.empty()) {
            Cell current = s.top();
            vector<int> dirs = { 0, 1, 2, 3 };
            shuffle(dirs.begin(), dirs.end(), g);
            bool moved = false;
            for (int i = 0; i < 4; i++) {
                int nx = current.x + dx[dirs[i]] * 2;
                int ny = current.y + dy[dirs[i]] * 2;
                if (isValid(nx, ny)) {
                    grid[current.y + dy[dirs[i]]][current.x + dx[dirs[i]]] = 1;
                    grid[ny][nx] = 1;
                    s.push(Cell(nx, ny));
                    moved = true;
                    break;
                }
            }
            if (!moved) s.pop();
        }
        grid[startPos.second][startPos.first] = 1;
        grid[endPos.second][endPos.first] = 1;
    }
};

// ---- Globalne zmienne ----
Maze maze(21, 21);

// Gracz - pozycja w przestrzeni 3D
float playerX, playerZ;        // pozycja (Y w OpenGL to góra/dół)
float yaw = 0.0f;            // obrót poziomy (myszka)
float pitch = 0.0f;            // obrót pionowy  (myszka)
const float PLAYER_HEIGHT = 0.5f;
const float CELL_SIZE = 1.0f;
const float WALL_HEIGHT = 1.2f;
const float MOVE_SPEED = 3.0f;
const float MOUSE_SENS = 0.08f;

double lastMouseX = 0, lastMouseY = 0;
bool firstMouse = true;

GLFWwindow* window;
float deltaTime = 0.0f, lastFrame = 0.0f;

// ---- Sprawdzanie kolizji ----
bool isWall(float wx, float wz) {
    int cx = (int)(wx / CELL_SIZE);
    int cz = (int)(wz / CELL_SIZE);
    if (cx < 0 || cx >= maze.width || cz < 0 || cz >= maze.height) return true;
    return maze.grid[cz][cx] == 0;
}

bool canMove(float nx, float nz) {
    float margin = 0.25f;
    // Sprawdź 4 rogi gracza
    return !isWall(nx - margin, nz - margin) &&
        !isWall(nx + margin, nz - margin) &&
        !isWall(nx - margin, nz + margin) &&
        !isWall(nx + margin, nz + margin);
}

// ---- Rysowanie geometrii ----
void drawQuad(float x1, float y1, float z1,
    float x2, float y2, float z2,
    float x3, float y3, float z3,
    float x4, float y4, float z4) {
    glBegin(GL_QUADS);
    glVertex3f(x1, y1, z1);
    glVertex3f(x2, y2, z2);
    glVertex3f(x3, y3, z3);
    glVertex3f(x4, y4, z4);
    glEnd();
}

void drawFloor() {
    glColor3f(0.25f, 0.22f, 0.18f); // ciemny brąz
    for (int z = 0; z < maze.height; z++) {
        for (int x = 0; x < maze.width; x++) {
            if (maze.grid[z][x] == 1) {
                float fx = x * CELL_SIZE;
                float fz = z * CELL_SIZE;
                // Podłoga
                glNormal3f(0, 1, 0);
                drawQuad(fx, 0, fz,
                    fx + CELL_SIZE, 0, fz,
                    fx + CELL_SIZE, 0, fz + CELL_SIZE,
                    fx, 0, fz + CELL_SIZE);
                // Sufit
                glColor3f(0.15f, 0.15f, 0.18f);
                glNormal3f(0, -1, 0);
                drawQuad(fx, WALL_HEIGHT, fz,
                    fx, WALL_HEIGHT, fz + CELL_SIZE,
                    fx + CELL_SIZE, WALL_HEIGHT, fz + CELL_SIZE,
                    fx + CELL_SIZE, WALL_HEIGHT, fz);
                glColor3f(0.25f, 0.22f, 0.18f);
            }
        }
    }
}

void drawWalls() {
    for (int z = 0; z < maze.height; z++) {
        for (int x = 0; x < maze.width; x++) {
            if (maze.grid[z][x] == 0) {
                float fx = x * CELL_SIZE;
                float fz = z * CELL_SIZE;

                // Ściany mają kolor kamienia z lekkim cieniowaniem
                float brightness = 0.55f + 0.05f * ((x + z) % 3);
                glColor3f(brightness * 0.9f, brightness * 0.85f, brightness * 0.75f);

                // Pełna ściana (6 ścian sześcianu)
                // Przód
                glNormal3f(0, 0, -1);
                drawQuad(fx, 0, fz,
                    fx + CELL_SIZE, 0, fz,
                    fx + CELL_SIZE, WALL_HEIGHT, fz,
                    fx, WALL_HEIGHT, fz);
                // Tył
                glNormal3f(0, 0, 1);
                drawQuad(fx, 0, fz + CELL_SIZE,
                    fx, WALL_HEIGHT, fz + CELL_SIZE,
                    fx + CELL_SIZE, WALL_HEIGHT, fz + CELL_SIZE,
                    fx + CELL_SIZE, 0, fz + CELL_SIZE);
                // Lewa
                glColor3f(brightness * 0.8f, brightness * 0.75f, brightness * 0.65f);
                glNormal3f(-1, 0, 0);
                drawQuad(fx, 0, fz,
                    fx, WALL_HEIGHT, fz,
                    fx, WALL_HEIGHT, fz + CELL_SIZE,
                    fx, 0, fz + CELL_SIZE);
                // Prawa
                glNormal3f(1, 0, 0);
                drawQuad(fx + CELL_SIZE, 0, fz,
                    fx + CELL_SIZE, 0, fz + CELL_SIZE,
                    fx + CELL_SIZE, WALL_HEIGHT, fz + CELL_SIZE,
                    fx + CELL_SIZE, WALL_HEIGHT, fz);
                // Góra ściany (ciemniejsza)
                glColor3f(brightness * 0.5f, brightness * 0.45f, brightness * 0.4f);
                glNormal3f(0, 1, 0);
                drawQuad(fx, WALL_HEIGHT, fz,
                    fx + CELL_SIZE, WALL_HEIGHT, fz,
                    fx + CELL_SIZE, WALL_HEIGHT, fz + CELL_SIZE,
                    fx, WALL_HEIGHT, fz + CELL_SIZE);
            }
        }
    }
}

void drawGoal() {
    // Cel - świecący słup na końcu labiryntu
    float gx = maze.endPos.first * CELL_SIZE + CELL_SIZE * 0.5f;
    float gz = maze.endPos.second * CELL_SIZE + CELL_SIZE * 0.5f;
    float t = (float)glfwGetTime();
    float pulse = 0.7f + 0.3f * sinf(t * 2.5f);

    glColor3f(1.0f * pulse, 0.2f, 0.2f * pulse);
    glBegin(GL_QUADS);
    float s = 0.15f;
    // Pionowy słup (uproszczony)
    glVertex3f(gx - s, 0, gz - s); glVertex3f(gx + s, 0, gz - s);
    glVertex3f(gx + s, WALL_HEIGHT, gz - s); glVertex3f(gx - s, WALL_HEIGHT, gz - s);

    glVertex3f(gx - s, 0, gz + s); glVertex3f(gx - s, WALL_HEIGHT, gz + s);
    glVertex3f(gx + s, WALL_HEIGHT, gz + s); glVertex3f(gx + s, 0, gz + s);

    glVertex3f(gx - s, 0, gz - s); glVertex3f(gx - s, 0, gz + s);
    glVertex3f(gx - s, WALL_HEIGHT, gz + s); glVertex3f(gx - s, WALL_HEIGHT, gz - s);

    glVertex3f(gx + s, 0, gz - s); glVertex3f(gx + s, WALL_HEIGHT, gz - s);
    glVertex3f(gx + s, WALL_HEIGHT, gz + s); glVertex3f(gx + s, 0, gz + s);
    glEnd();
}

void drawMinimap() {
    int W, H;
    glfwGetFramebufferSize(window, &W, &H);

    // Przełącz na 2D
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, W, H, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);

    int mapOffX = W - maze.width * 4 - 8;
    int mapOffY = 8;
    int cs = 4; // piksel na komórkę

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
    // Gracz na mapie
    int px = (int)(playerX / CELL_SIZE);
    int pz = (int)(playerZ / CELL_SIZE);
    glColor3f(0.0f, 1.0f, 0.3f);
    glBegin(GL_QUADS);
    glVertex2i(mapOffX + px * cs, mapOffY + pz * cs);
    glVertex2i(mapOffX + px * cs + cs, mapOffY + pz * cs);
    glVertex2i(mapOffX + px * cs + cs, mapOffY + pz * cs + cs);
    glVertex2i(mapOffX + px * cs, mapOffY + pz * cs + cs);
    glEnd();
    // Cel
    glColor3f(1.0f, 0.2f, 0.2f);
    int ex = maze.endPos.first, ez = maze.endPos.second;
    glBegin(GL_QUADS);
    glVertex2i(mapOffX + ex * cs, mapOffY + ez * cs);
    glVertex2i(mapOffX + ex * cs + cs, mapOffY + ez * cs);
    glVertex2i(mapOffX + ex * cs + cs, mapOffY + ez * cs + cs);
    glVertex2i(mapOffX + ex * cs, mapOffY + ez * cs + cs);
    glEnd();

    // Celownik (krzyżyk w środku ekranu)
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

// ---- Obsługa wejścia ----
void mouseCallback(GLFWwindow* w, double xpos, double ypos) {
    if (firstMouse) {
        lastMouseX = xpos; lastMouseY = ypos;
        firstMouse = false;
    }
    float dx_ = (float)(xpos - lastMouseX) * MOUSE_SENS;
    float dy_ = (float)(ypos - lastMouseY) * MOUSE_SENS;
    lastMouseX = xpos; lastMouseY = ypos;

    yaw -= dx_; //lewo prawo
    pitch -= dy_; // gora dol
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
}

void processInput() {
    float rad = yaw * 3.14159f / 180.0f;

    // Kierunek "przód"
    float fwdX = sinf(rad);
    float fwdZ = cosf(rad);

    // Kierunek "w bok" (wektor prostopadły)
    // Zmieniamy znaki, aby D było w prawo, a A w lewo
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

    // TUTAJ POPRAWKA:
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        nx += sideX * speed; nz += sideZ * speed; // A teraz powinno iść w lewo
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        nx -= sideX * speed; nz -= sideZ * speed; // D teraz powinno iść w prawo
    }

    if (canMove(nx, playerZ)) playerX = nx;
    if (canMove(playerX, nz)) playerZ = nz;
}

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
        if (key == GLFW_KEY_TAB) {
            // Przełącz kursor myszy
            int mode = glfwGetInputMode(w, GLFW_CURSOR);
            glfwSetInputMode(w, GLFW_CURSOR,
                mode == GLFW_CURSOR_DISABLED ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
        }
    }
}

// ---- Konfiguracja oświetlenia ----
void setupLighting() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    GLfloat ambientLight[] = { 0.25f, 0.22f, 0.20f, 1.0f };
    GLfloat diffuseLight[] = { 0.85f, 0.80f, 0.70f, 1.0f };
    GLfloat specularLight[] = { 0.3f, 0.3f, 0.3f, 1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specularLight);

    // Ograniczenie zasięgu - efekt latarki
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 0.5f);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.3f);
    glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.05f);
}

void updateLight() {
    // Światło podąża za graczem (latarka)
    GLfloat pos[] = { playerX, PLAYER_HEIGHT + 0.2f, playerZ, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, pos);
}

// ---- Program główny ----
int main() {
    srand((unsigned)time(0));
    maze.generateMaze();

    if (!glfwInit()) return -1;

    window = glfwCreateWindow(1024, 768, "Labirynt 3D - WASD/Strzalki + Myszka | R=nowy | TAB=kursor | ESC=wyjscie", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED); // ukryj kursor

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);

    // Mgła - klimat lochów!
    glEnable(GL_FOG);
    GLfloat fogColor[] = { 0.0f, 0.0f, 0.02f, 1.0f };
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 2.5f);
    glFogf(GL_FOG_END, 8.0f);

    setupLighting();

    // Startowa pozycja gracza
    playerX = (maze.startPos.first + 0.5f) * CELL_SIZE;
    playerZ = (maze.startPos.second + 0.5f) * CELL_SIZE;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput();

        int W, H;
        glfwGetFramebufferSize(window, &W, &H);
        glViewport(0, 0, W, H);

        glClearColor(0.0f, 0.0f, 0.02f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Projekcja
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(75.0, (double)W / H, 0.05, 50.0);

        // Kamera
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

        // Rysuj świat
        drawFloor();
        drawWalls();
        drawGoal();

        // HUD (minimap + celownik) - bez oświetlenia
        glDisable(GL_LIGHTING);
        glDisable(GL_FOG);
        drawMinimap();
        glEnable(GL_LIGHTING);
        glEnable(GL_FOG);

        // Sprawdź czy gracz dotarł do celu
        float dx2 = playerX - (maze.endPos.first + 0.5f) * CELL_SIZE;
        float dz2 = playerZ - (maze.endPos.second + 0.5f) * CELL_SIZE;
        if (sqrtf(dx2 * dx2 + dz2 * dz2) < 0.6f) {
            // Nowy labirynt przy dotarciu do celu
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