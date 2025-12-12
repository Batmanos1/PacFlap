#include "raylib.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

// ==========================================
//          GLOBAL CONFIGURATION
// ==========================================

#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 450
#define FPS           60

// PLAYER SETTINGS
#define PACMAN_RADIUS 20.0f
#define JUMP_STRENGTH -5.0f
#define PACMAN_X_POS  SCREEN_WIDTH / 4.0f

// PIPE SETTINGS
#define PIPE_WIDTH    70
#define MAX_PIPES     100  // Maximum buffer size (safety limit)
#define MAX_LEVELS    2    // Total number of levels

// DATA STRUCTURE FOR A LEVEL
typedef struct {
    int pipeCount;      // How many pipes to pass to beat level
    int score;          // Level Score
    float speed;        // How fast the pipes move
    float gapSize;      // Vertical space between pipes
    float gravity;      // Gravity strength
    Color color;        // Pipe color
} LevelData;

LevelData levels[MAX_LEVELS]; // Array to hold our levels

// ==========================================
//          LEVEL SETUP
// ==========================================

void SetupLevels() {
    // ---------------- LEVEL 1 ----------------
    levels[0].score     = 0;
    levels[0].pipeCount = 5;
    levels[0].speed     = 3.0f;
    levels[0].gapSize   = 160.0f;
    levels[0].gravity   = 0.4f;
    levels[0].color     = SKYBLUE;

    // ---------------- LEVEL 2 (Moving Pipes) ----------------
    levels[1].score     = 0;
    levels[1].pipeCount = 10;
    levels[1].speed     = 3.5f;
    levels[1].gapSize   = 160.0f;
    levels[1].gravity   = 0.45f;
    levels[1].color     = LIME;

    // ---------------- LEVEL 3 (not used) ----------------
    levels[2].score     = 0;
    levels[2].pipeCount = 20;
    levels[2].speed     = 5.0f;
    levels[2].gapSize   = 120.0f;
    levels[2].gravity   = 0.5f;
    levels[2].color     = RED;
}

// ==========================================
//          GAME VARIABLES
// ==========================================

// State
int currentLevel = 0;
bool gameStarted = false;
bool gameOver = false;
bool levelComplete = false;
bool gameVictory = false;

// Entities
float pacmanY;
float pacmanVelocityY;
float currentMouthAngle = 45.0f;
float animationTime = 0.0f;

// Pipe Arrays
float pipeX[MAX_PIPES];
float pipeGapY[MAX_PIPES];
float initialPipeGapY[MAX_PIPES]; // Stores the starting Y for sine wave calculations
bool pipePassed[MAX_PIPES];
bool orbCollected[MAX_PIPES];

// Orb Arrays (New)
// The "Relative" Y position.
// if orbRelY is 50, the orb is 50px below the top pipe.
float orbRelY[MAX_PIPES];

// Scores
int currentScore = 0;
int totalScore = 0;

// ==========================================
//            LOGIC FUNCTIONS
// ==========================================

void ResetEntityPositions() {
    LevelData cur = levels[currentLevel];

    pacmanY = SCREEN_HEIGHT / 2.0f;
    pacmanVelocityY = 0;
    animationTime = 0;

    // Generate Pipes based on current level settings
    for (int i = 0; i < cur.pipeCount; i++) {
        pipeX[i] = SCREEN_WIDTH + 300 + (i * 300); // Start off-screen

        int minGap = 50;
        int maxGap = SCREEN_HEIGHT - 50 - (int)cur.gapSize;
        if (maxGap < minGap) maxGap = minGap + 10; // Safety check

        // Store the random position for the gap
        float randomY = minGap + rand() % (maxGap - minGap);

        pipeGapY[i] = randomY;
        initialPipeGapY[i] = randomY; // Save this for the sine wave reference

        // --- CALCULATE ORB POSITION ---
        // somewhere inside the gap.
        // a padding of 20px so it's not inside the wall.
        orbCollected[i] = false; // Reset orb state
        int padding = 20;
        int safeRange = (int)cur.gapSize - (padding * 2);

        if (safeRange > 0) {
            orbRelY[i] = padding + (rand() % safeRange);
        } else {
            orbRelY[i] = cur.gapSize / 2; // Exact center if gap is tight
        }

        pipePassed[i] = false;
    }
}

// Handles switching levels or restarting
void ChangeState(int newLevelIndex) {
    if (newLevelIndex >= MAX_LEVELS) {
        gameVictory = true;
        return;
    }

    currentLevel = newLevelIndex;
    gameStarted = false;
    gameOver = false;
    levelComplete = false;

    ResetEntityPositions();
}


// ==========================================
//         UPDATE GAME (CHECKS)
// ==========================================

void UpdateGame() {
    if (!gameStarted || gameOver || levelComplete || gameVictory) return;

    LevelData cur = levels[currentLevel];

    // 1. Update Player
    pacmanVelocityY += cur.gravity;
    if (IsKeyPressed(KEY_SPACE)) pacmanVelocityY = JUMP_STRENGTH;
    pacmanY += pacmanVelocityY;

    // Animation
    animationTime += GetFrameTime() * 10.0f;
    currentMouthAngle = 25.0f + 20.0f * sinf(animationTime);

    // Floor/Ceiling Collision
    if (pacmanY - PACMAN_RADIUS <= 0 || pacmanY + PACMAN_RADIUS >= SCREEN_HEIGHT) {
        gameOver = true;
    }

    // 2. Update Pipes
    int pipesCleared = 0;

    for (int i = 0; i < cur.pipeCount; i++) {
        pipeX[i] -= cur.speed;

        // --- LEVEL 2 MECHANIC: SINE WAVE PIPES ---
        if (currentLevel == 1) {
            float time = GetTime();
            float amplitude = 50.0f; // Range of motion (up/down pixels)
            float speed = 3.0f;      // Speed of oscillation

            // We use 'i' in the sine calculation to offset the waves so they don't move in unison
            pipeGapY[i] = initialPipeGapY[i] + sinf(time * speed + i) * amplitude;
        }

        // Collision Check
        Rectangle topPipe = { pipeX[i], 0, PIPE_WIDTH, pipeGapY[i] };
        Rectangle botPipe = { pipeX[i], pipeGapY[i] + cur.gapSize, PIPE_WIDTH, SCREEN_HEIGHT };
        Rectangle player  = { PACMAN_X_POS - PACMAN_RADIUS + 5, pacmanY - PACMAN_RADIUS + 5, PACMAN_RADIUS*2 - 10, PACMAN_RADIUS*2 - 10 };

        if (CheckCollisionRecs(topPipe, player) || CheckCollisionRecs(botPipe, player)) {
            gameOver = true;
        }

        // --- ORB COLLISION ---
        // Only check if not already collected
        if (!orbCollected[i]) {
            Rectangle orbHitbox = {
                pipeX[i] + (PIPE_WIDTH / 2) - 5,
                pipeGapY[i] + orbRelY[i] - 5,
                10, 10
            };

            if (CheckCollisionRecs(player, orbHitbox)) {
                orbCollected[i] = true; // Mark as collected
                levels[currentLevel].score += 5; // Increase points
                currentScore += 5; // update points
            }
        }

        // Pipes cleared check
        if (!pipePassed[i] && pipeX[i] + PIPE_WIDTH < PACMAN_X_POS) {
            pipePassed[i] = true;
            levels[currentLevel].score ++; // Increase points
            currentScore ++; // update points
        }

        if (pipePassed[i]) pipesCleared++;
    }

    if (pipesCleared >= cur.pipeCount) {
        levelComplete = true;
        totalScore+=levels[currentLevel].score;
    }
}


// ==========================================
//         DRAW GAME (SHAPES)
// ==========================================

void DrawGame() {
    BeginDrawing();
    ClearBackground(BLACK);

    LevelData cur = levels[currentLevel];
    int border = 4; // Outline thickness

    // 1. DRAW PIPES and ORBS
    for (int i = 0; i < cur.pipeCount; i++) {
        if (pipeX[i] > -PIPE_WIDTH && pipeX[i] < SCREEN_WIDTH) {
            // Top Pipe
            DrawRectangle(pipeX[i], 0, PIPE_WIDTH, pipeGapY[i], cur.color);
            DrawRectangle(pipeX[i] + border, 0, PIPE_WIDTH - border*2, pipeGapY[i] - border, BLACK);

            // Bottom Pipe
            float bottomY = pipeGapY[i] + cur.gapSize;
            float bottomHeight = SCREEN_HEIGHT - bottomY;
            DrawRectangle(pipeX[i], bottomY, PIPE_WIDTH, bottomHeight, cur.color);
            DrawRectangle(pipeX[i] + border, bottomY + border, PIPE_WIDTH - border*2, bottomHeight - border, BLACK);

            // Orbs
            // Position = Top of gap + Calculated Offset
            // Only draw if NOT collected
            if (!orbCollected[i]) {
                float finalOrbY = pipeGapY[i] + orbRelY[i];
                DrawCircle(pipeX[i] + (PIPE_WIDTH/2), finalOrbY, 5, WHITE);
            }
        }
    }

    // 2. DRAW PAC-MAN

    // Calculate rotation based on velocity
    // Multiplier 3.0f makes the rotation more noticeable.
    float tilt = pacmanVelocityY * 3.0f;

    // Clamp the rotation so it doesn't look completely backwards or breaks its neck
    if (tilt > 35.0f) tilt = 35.0f;   // Max looking down
    if (tilt < -25.0f) tilt = -25.0f; // Max looking up

    // Yellow circle being's shape
    DrawCircleSector((Vector2){PACMAN_X_POS, pacmanY}, PACMAN_RADIUS,

                      currentMouthAngle + tilt,            // Start Angle
                      (360.0f - currentMouthAngle) + tilt, // End Angle
                      0, YELLOW);


    // 3. UI TEXT
    if (gameOver) {
        DrawText("GAME OVER", 300, 200, 40, RED);
        DrawText("Press SPACE to Retry", 280, 250, 20, WHITE);
    }
        else if (gameVictory) {
        DrawText("YOU WIN!", 300, 200, 40, GOLD);
        DrawText("Press SPACE to Restart Game", 260, 250, 20, WHITE);
    }
    else if (levelComplete) {
        DrawText("LEVEL COMPLETE!", 250, 200, 40, GREEN);
        DrawText("Press SPACE for Next Level", 260, 250, 20, WHITE);
    }
    else if (!gameStarted) {
        DrawText(TextFormat("LEVEL %d", currentLevel + 1), 340, 180, 30, cur.color);
        DrawText("Press SPACE to Start", 290, 230, 20, WHITE);
    }

    DrawText(TextFormat("Lvl: %d", currentLevel + 1), 10, 10, 20, WHITE);
    DrawText (TextFormat ("Score: %d", currentScore), 10, 40, 20, YELLOW);

    EndDrawing();
}


// ==========================================
//              MAIN
// ==========================================

int main(void) {
    srand(time(NULL));

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "IDAPP");
    SetTargetFPS(FPS);

    SetupLevels();
    ChangeState(0); // Load Level 1

    while (!WindowShouldClose()) {

        // Input Handling
        if (IsKeyPressed(KEY_SPACE)) {
            if (!gameStarted && !gameOver && !levelComplete && !gameVictory) {
                gameStarted = true;
                pacmanVelocityY = JUMP_STRENGTH;
            }
            else if (gameOver) {
                ChangeState(currentLevel);
                currentScore = totalScore; // Retry same level
                levels[currentLevel].score = 0; // Reset level point
            }
            else if (gameVictory) {
                ChangeState(currentLevel + 1);
                totalScore += currentScore+totalScore;
            }
            else if (levelComplete) {
                ChangeState(currentLevel + 1);
                totalScore += currentScore-totalScore; // Next level
            }

        }

        UpdateGame();
        DrawGame();
    }

    CloseWindow();
    return 0;
}
