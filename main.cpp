// infinite_lanes_centered_fixed.cpp
#include <GL/glut.h>
#include <cmath>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <deque>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

// --- STRUCTS ---
struct Car { float x, y; float speed; };

// --- PLAYER ---
float playerX = 0.0f, playerY = 0.0f;
float playerSpeed = 0.0f, playerAcc = 0.0001f;
float rotSmooth = 0.0f;
float drift = 0.0f;
float carWidth = 0.1f, carHeight = 0.2f;

bool keyStates[256] = { 0 };
bool specialKeyStates[256] = { 0 };
bool gameOver = false;


// --- TRAIL ---
std::deque<std::pair<float, float>> trail;
const int TRAIL_MAX = 14;
const float TRAIL_MIN_DIST = 0.03f;
float trail_last_x = 0.0f, trail_last_y = 0.0f;

// --- LANES (centered) ---
float laneWidth = 0.6f;            // width of one lane
std::vector<float> laneCenters;    // centers where cars will spawn/drive
int laneNumLeft = 12, laneNumRight = 12; // how many lanes each side

// --- AI ---
std::vector<Car> aiCars;
#define NUM_AI_CARS 50
#define AI_MIN_Y 0.5f
#define AI_MAX_Y 8.0f
#define AI_SPEED 0.008f
#define AI_SPAWN_AHEAD_MIN 2.0f
#define AI_SPAWN_AHEAD_MAX 4.0f

float frand(float a, float b) { return a + static_cast<float>(rand()) / RAND_MAX * (b - a); }

// --- REWARDS ---
struct Reward { float x, y; bool collected; };
std::vector<Reward> rewards;
int score = 0;

void spawnReward() {
    if (laneCenters.empty()) return;
    Reward r;
    r.x = laneCenters[rand() % laneCenters.size()];  // use lane centers
    r.y = playerY + frand(2.0f, 5.0f);
    r.collected = false;
    rewards.push_back(r);
}


// --- TEXTURE ---
GLuint carTexture = 0;
GLuint rewardTexture = 0;
GLuint loadTexture(const char* filename) {
    int width, height, channels;
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 4);
    if (!data) { std::cerr << "Failed to load texture: " << filename << std::endl; return 0; }
    GLuint tex; glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
    return tex;
}


void drawRewards() {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, rewardTexture);
    for (auto& r : rewards) {
        if (r.collected) continue;
        float s = 0.1;
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(r.x - s, r.y - s);
        glTexCoord2f(1, 0); glVertex2f(r.x + s, r.y - s);
        glTexCoord2f(1, 1); glVertex2f(r.x + s, r.y + s);
        glTexCoord2f(0, 1); glVertex2f(r.x - s, r.y + s);
        glEnd();
    }
    glDisable(GL_TEXTURE_2D);
}

// --- LANE INIT (centers) ---
void initLanes(int numLeft = 12, int numRight = 12, float width = 0.6f) {
    laneCenters.clear();
    laneWidth = width;
    laneNumLeft = numLeft;
    laneNumRight = numRight;
    for (int i = -numLeft; i <= numRight; ++i) {
        float center = i * laneWidth + laneWidth * 0.5f;
        laneCenters.push_back(center);
    }
}

// --- RESET ---
void resetGame() {
    playerX = 0.0f; playerY = 0.0f;
    playerSpeed = 0.0f; drift = 0.0f; rotSmooth = 0.0f;
    gameOver = false; trail.clear(); aiCars.clear(); rewards.clear(); score = 0;
    if (laneCenters.empty()) initLanes(laneNumLeft, laneNumRight, laneWidth);
    // spawn many AI on lane centers (y random ahead)
    for (int i = 0; i < NUM_AI_CARS; ++i) {
        Car c;
        c.x = laneCenters[rand() % laneCenters.size()];
        c.y = frand(AI_MIN_Y, AI_MAX_Y);
        c.speed = AI_SPEED * frand(0.9f, 1.4f);
        aiCars.push_back(c);
    }
    // spawn some initial rewards
    for (int i = 0; i < 8; ++i) spawnReward();
}

// --- DRAW CAR ---
void drawCarTextured(float x, float y, float angle) {
    glPushMatrix();
    glTranslatef(x, y, 0.0f);
    glRotatef(angle, 0, 0, 1);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, carTexture);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(-carWidth / 2, -carHeight / 2);
    glTexCoord2f(1, 0); glVertex2f(carWidth / 2, -carHeight / 2);
    glTexCoord2f(1, 1); glVertex2f(carWidth / 2, carHeight / 2);
    glTexCoord2f(0, 1); glVertex2f(-carWidth / 2, carHeight / 2);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glPopMatrix();
}

// --- TRAIL ---
void drawTrailVisual() {
    if (trail.empty()) return;
    int n = (int)trail.size(), i = 0;
    for (auto& p : trail) {
        float t = (float)i / std::max(1, n - 1);
        float alpha = 0.3f + 0.7f * t;   // mai vizibil
        float scale = 0.55f + 0.7f * t;
        float px = p.first, py = p.second;
        float w = (carWidth * 0.3f) * scale;
        float h = (carHeight * 0.25f) * scale;
        glColor4f(0.15f, 0.15f, 0.15f, alpha); // trail cu gri închis
        glBegin(GL_QUADS);
        glVertex2f(px - w, py - h); glVertex2f(px + w, py - h);
        glVertex2f(px + w, py + h); glVertex2f(px - w, py + h);
        glEnd();
        ++i;
    }
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f); // reset color
}

// --- DRAW LANES (draw lane boundaries; cars sit on lane centers) ---
void drawLanes(float camY) {
    if (laneCenters.empty()) return;
    glColor3f(1.0f, 0.85f, 0.0f); // galben 
    glEnable(GL_LINE_STIPPLE);
    glLineStipple(1, 0x00FF);
    glLineWidth(5.0f);
    glBegin(GL_LINES);

    int drawLeft = laneNumLeft;
    int drawRight = laneNumRight;

    float startY = camY - 4.0f; // aplicăm scroll
    float endY = camY + 12.0f;

    for (int i = -drawLeft; i <= drawRight; ++i) {
        float x = i * laneWidth;
        glVertex2f(x, startY);
        glVertex2f(x, endY);
    }

    glEnd();
    glDisable(GL_LINE_STIPPLE);
}


// --- UPDATE ---
// --- UPDATE SMOOTH PLAYER MOVEMENT ---
void update() {
    if (gameOver) {
        glutPostRedisplay();
        return;
    }

    // --- PLAYER ACCELERATION ---
    float maxSpeed = 0.02f;  // viteza maximă

    // accelerezi doar dacă apeși W/UP
    if (specialKeyStates[GLUT_KEY_UP] || keyStates['w'] || keyStates['W']) {
        playerSpeed += playerAcc;
        if (playerSpeed > maxSpeed) playerSpeed = maxSpeed;
    }
    // frânare doar dacă apeși S/DOWN
    //else if (specialKeyStates[GLUT_KEY_DOWN] || keyStates['s'] || keyStates['S']) {
    //    playerSpeed -= playerAcc * 2.0f;  // frânezi mai rapid
    //    if (playerSpeed < 0.0f) playerSpeed = 0.0f;
    //}
    // dacă nu apeși nimic: player se oprește imediat
    else {
        playerSpeed = 0.0f;
    }

    // --- LATERAL MOVEMENT & DRIFT ---
    if (specialKeyStates[GLUT_KEY_LEFT] || keyStates['a'] || keyStates['A']) {
        playerX -= 0.009f;
        drift += 0.05f;
        if (drift > 10.0f) drift = 10.0f;
    }
    else if (specialKeyStates[GLUT_KEY_RIGHT] || keyStates['d'] || keyStates['D']) {
        playerX += 0.009f;
        drift -= 0.05f;
        if (drift < -10.0f) drift = -10.0f;
    }
    else {
        drift *= 0.9f; // revenire treptată la centru
    }

    rotSmooth = rotSmooth * 0.9f + drift * 0.1f;

    // --- WALLS: block player at edges ---
    float leftLimit = -laneNumLeft * laneWidth + carWidth / 2.0f;
    float rightLimit = laneNumRight * laneWidth - carWidth / 2.0f;

    if (playerX < leftLimit) {
        playerX = leftLimit;
        drift = 0.0f;        // optional: oprește drift-ul lateral
        rotSmooth = 0.0f;
    }
    else if (playerX > rightLimit) {
        playerX = rightLimit;
        drift = 0.0f;
        rotSmooth = 0.0f;
    }



    // --- UPDATE Y ---
    playerY += playerSpeed ;

    // --- AI ---
    for (auto& c : aiCars) {
        c.y -= c.speed;
        if (c.y < playerY - 2.0f) {
            bool overlap = false; int attempts = 0, MAX_ATT = 12;
            do {
                overlap = false;
                if (!laneCenters.empty()) c.x = laneCenters[rand() % laneCenters.size()];
                c.y = playerY + frand(AI_SPAWN_AHEAD_MIN, AI_SPAWN_AHEAD_MAX);
                for (auto& o : aiCars) {
                    if (&o == &c) continue;
                    if (fabsf(o.x - c.x) < carWidth * 1.05f && fabsf(o.y - c.y) < carHeight * 1.2f) { overlap = true; break; }
                }
                ++attempts;
            } while (overlap && attempts < MAX_ATT);
        }

        if (fabsf(playerX - c.x) < carWidth && fabsf(playerY - c.y) < carHeight) {
            gameOver = true;
            std::cout << "GAME OVER!" << std::endl;
        }
    }

    // --- REWARDS ---
    const float REWARD_SPEED = 0.008f; // același principiu ca AI_SPEED, poți ajusta

    for (auto& r : rewards) {
        r.y -= REWARD_SPEED;
        if (!r.collected && fabsf(playerX - r.x) < carWidth / 2.0f && fabsf(playerY - r.y) < carHeight / 2.0f) {
            r.collected = true;
            score += 1;
            std::cout << "+1 Score! Total: " << score << std::endl;
        }
    }

    // apoi șterge recompensele colectate sau prea jos
    rewards.erase(
        std::remove_if(rewards.begin(), rewards.end(),
            [](Reward& r) { return r.collected || r.y < playerY - 5.0f; }),
        rewards.end()
    );

    // spawn dacă e nevoie
    while (rewards.size() < 8) { // păstrează întotdeauna un minim de recompense
        spawnReward();
    }

    // --- TRAIL ---
    float tx = playerX;
    float ty = playerY - carHeight * 0.35f;
    float dx = tx - trail_last_x, dy = ty - trail_last_y;
    if (trail.empty() || (dx * dx + dy * dy) >= (TRAIL_MIN_DIST * TRAIL_MIN_DIST)) {
        trail.emplace_back(tx, ty);
        trail_last_x = tx; trail_last_y = ty;
        if ((int)trail.size() > TRAIL_MAX) trail.pop_front();
    }

    
    glutPostRedisplay();
}



// --- HUD ---
void drawHUD() {
    glPushMatrix();
    glLoadIdentity();
    glColor3f(1.0f, 0.0f, 0.0f); // roșu

    char buf[32];
    sprintf_s(buf, sizeof(buf), "Score: %d", score);

    // calculăm colțul stâng-sus în funcție de aspect și zoom
    float aspect = 1280.0f / 720.0f; // width/height
    float zoom = 2.0f;               // trebuie să fie exact ce-ai folosit la gluOrtho2D
    float x = -zoom * aspect + 0.05f; // puțin offset spre interior
    float y = zoom - 0.1f;            // puțin offset jos din top

    glRasterPos2f(x, y);
    for (int i = 0; buf[i] != '\0'; ++i)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, buf[i]);

    if (gameOver) {
        const char* msg = "GAME OVER! Press R to restart";
        glColor3f(1.0f, 0.2f, 0.2f);
        glRasterPos2f(-1.3f, 0.0f); // centrat aproximativ
        for (int i = 0; msg[i]; ++i)
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, msg[i]);
    }

    glPopMatrix();
}


// --- RENDER ---
void renderScene() {
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();

    // camera follow
    static float camX = 0.0f, camY = 0.0f;
    camX = camX * 0.9f + playerX * 0.1f;
    camY = camY * 0.9f + playerY * 0.1f;
    glTranslatef(-camX, -camY, 0.0f);

    // lanes (draw boundaries), rewards, AI, trail, player
    drawLanes(camY);
    drawRewards();
    for (auto& c : aiCars) drawCarTextured(c.x, c.y, 0.0f);
    drawTrailVisual();
    drawCarTextured(playerX, playerY, -rotSmooth);

    drawHUD();

    glutSwapBuffers();
}

// --- INPUT ---
void handleKeyDown(unsigned char key, int, int) { keyStates[key] = true; if (gameOver && (key == 'r' || key == 'R')) resetGame(); }
void handleKeyUp(unsigned char key, int, int) { keyStates[key] = false; }
void handleSpecialDown(int key, int, int) { specialKeyStates[key] = true; }
void handleSpecialUp(int key, int, int) { specialKeyStates[key] = false; }

// --- INIT ---
void init() {
    srand((unsigned)time(0));
    glClearColor(0.6f, 0.6f, 0.6f, 1.0f); // fundal gri deschis
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // zoom / viewport
    float aspect = 1280.0f / 720.0f;
    float zoom = 2.0f;
    gluOrtho2D(-zoom * aspect, zoom * aspect, -zoom, zoom);

    glMatrixMode(GL_MODELVIEW);

    // load texture for all cars (adjust path if needed)
    carTexture = loadTexture("C:\\Users\\Mihai\\Downloads\\car.png");
    if (carTexture == 0) {
        std::cerr << "Failed to load car.png. Check the path." << std::endl;
        exit(1);
    }
    rewardTexture = loadTexture("C:\\Users\\Mihai\\Downloads\\coin.png");
    if (rewardTexture == 0) {
       std::cerr << "Failed to load coin.png" << std::endl;
       exit(1);
    }

    initLanes(18, 18, 0.6f);
    resetGame();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(1280, 720);
    glutCreateWindow("Infinite Lanes Racing - centered cars");
    init();
    glutDisplayFunc(renderScene);
    glutIdleFunc(update);
    glutKeyboardFunc(handleKeyDown);
    glutKeyboardUpFunc(handleKeyUp);
    glutSpecialFunc(handleSpecialDown);
    glutSpecialUpFunc(handleSpecialUp);
    glutMainLoop();
    return 0;
}
