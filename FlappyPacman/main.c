#include "raylib.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

// ==========================================
//          GLOBAL CONFIGURATION
// ==========================================

#define SCREEN_WIDTH  800
#define SCREEN_HEIGHT 450
#define FPS           60

// PLAYER SETTINGS
#define PACMAN_RADIUS 20.0f
#define JUMP_STRENGTH -6.0f
#define PACMAN_X_POS  SCREEN_WIDTH / 4.0f

// PIPE SETTINGS
#define PIPE_WIDTH    70
#define MAX_PIPES     100
#define MAX_LEVELS    2
#define MAX_PLAYERS   10

// ==========================================
//          DATA STRUCTURES
// ==========================================

// Game State Enum
typedef enum {
    STATE_INPUT,        // Entering name
    STATE_TITLE,        // Press Space to start level
    STATE_PLAYING,      // Game active
    STATE_LEVEL_DONE,   // Level beat
    STATE_GAMEOVER,     // Died
    STATE_VICTORY       // All levels beat (Scoreboard)
} GameState;

typedef struct {
    int pipeCount;
    float speed;
    float gapSize;
    float gravity;
    Color color;
} LevelData;

typedef struct {
    char name[16];
    int score;
    bool active;
} PlayerData;

LevelData levels[MAX_LEVELS];
PlayerData players[MAX_PLAYERS];
int playerCount = 0;

// ==========================================
//          GAME VARIABLES
// ==========================================

GameState currentState = STATE_INPUT;
int currentLevel = 0;

// Current Player Info
char tempName[16] = "\0";
int letterCount = 0;
int currentSessionScore = 0;
int levelStartScore = 0;

// Entities
float pacmanY;
float pacmanVelocityY;
float currentMouthAngle = 45.0f;
float animationTime = 0.0f;

// Pipe Arrays
float pipeX[MAX_PIPES];
float pipeGapY[MAX_PIPES];
float initialPipeGapY[MAX_PIPES];
bool pipePassed[MAX_PIPES];
bool orbCollected[MAX_PIPES];
float orbRelY[MAX_PIPES];

// ==========================================
//          SETUP FUNCTIONS
// ==========================================

void SetupLevels() {
    // ---------------- LEVEL 1 ----------------
    levels[0].pipeCount = 5;
    levels[0].speed     = 3.0f;
    levels[0].gapSize   = 160.0f;
    levels[0].gravity   = 0.4f;
    levels[0].color     = SKYBLUE;

    // ---------------- LEVEL 2 (Moving Pipes) ----------------
    levels[1].pipeCount = 10;
    levels[1].speed     = 3.5f;
    levels[1].gapSize   = 150.0f;
    levels[1].gravity   = 0.45f;
    levels[1].color     = LIME;
}

void ResetEntityPositions() {
    LevelData cur = levels[currentLevel];

    pacmanY = SCREEN_HEIGHT / 2.0f;
    pacmanVelocityY = 0;
    animationTime = 0;

    // Generate Pipes
    for (int i = 0; i < cur.pipeCount; i++) {
        pipeX[i] = SCREEN_WIDTH + 300 + (i * 300);

        int minGap = 50;
        int maxGap = SCREEN_HEIGHT - 50 - (int)cur.gapSize;
        if (maxGap < minGap) maxGap = minGap + 10;

        float randomY = minGap + rand() % (maxGap - minGap);

        pipeGapY[i] = randomY;
        initialPipeGapY[i] = randomY;

        // Orb Logic
        orbCollected[i] = false;
        int padding = 20;
        int safeRange = (int)cur.gapSize - (padding * 2);

        if (safeRange > 0) {
            orbRelY[i] = padding + (rand() % safeRange);
        } else {
            orbRelY[i] = cur.gapSize / 2;
        }

        pipePassed[i] = false;
    }
}

// ==========================================
//          UPDATE LOGIC
// ==========================================

void UpdateInput() {
    int key = GetCharPressed();

    while (key > 0) {
        if ((key >= 32) && (key <= 125) && (letterCount < 15)) {
            tempName[letterCount] = (char)key;
            tempName[letterCount+1] = '\0';
            letterCount++;
        }
        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        letterCount--;
        if (letterCount < 0) letterCount = 0;
        tempName[letterCount] = '\0';
    }

    if (IsKeyPressed(KEY_ENTER) && letterCount > 0) {
        currentState = STATE_TITLE;
        currentSessionScore = 0;
        levelStartScore = 0;
        currentLevel = 0;
        ResetEntityPositions();
    }
}

void UpdateGame() {
    if (currentState == STATE_INPUT) {
        UpdateInput();
        return;
    }

    // State Transitions via Spacebar
    if (IsKeyPressed(KEY_SPACE)) {
        if (currentState == STATE_TITLE) {
            currentState = STATE_PLAYING;
            pacmanVelocityY = JUMP_STRENGTH;
        }
        else if (currentState == STATE_LEVEL_DONE) {
            currentLevel++;
            if (currentLevel >= MAX_LEVELS) {
                // VICTORY LOGIC
                currentState = STATE_VICTORY;

                // 1. Add Player to Scoreboard
                if (playerCount < MAX_PLAYERS) {
                    strcpy(players[playerCount].name, tempName);
                    players[playerCount].score = currentSessionScore;
                    players[playerCount].active = true;
                    playerCount++;
                }

                // 2. SORT SCOREBOARD (Bubble Sort: Highest to Lowest)
                for (int i = 0; i < playerCount - 1; i++) {
                    for (int j = 0; j < playerCount - i - 1; j++) {
                        if (players[j].score < players[j+1].score) {
                            // Swap entire struct
                            PlayerData temp = players[j];
                            players[j] = players[j+1];
                            players[j+1] = temp;
                        }
                    }
                }

            } else {
                currentState = STATE_TITLE;
                levelStartScore = currentSessionScore;
                ResetEntityPositions();
            }
        }
        else if (currentState == STATE_GAMEOVER) {
            // Retry Level
            currentSessionScore = levelStartScore;
            currentState = STATE_TITLE;
            ResetEntityPositions();
        }
        else if (currentState == STATE_VICTORY) {
            // Return to input screen for new player
            currentState = STATE_INPUT;
            letterCount = 0;
            tempName[0] = '\0';
        }
    }

    if (currentState != STATE_PLAYING) return;

    LevelData cur = levels[currentLevel];

    // 1. Update Player
    pacmanVelocityY += cur.gravity;
    if (IsKeyPressed(KEY_SPACE)) pacmanVelocityY = JUMP_STRENGTH;
    pacmanY += pacmanVelocityY;

    // Animation
    animationTime += GetFrameTime() * 10.0f;
    currentMouthAngle = 25.0f + 20.0f * sinf(animationTime);

    // Bounds Collision
    if (pacmanY - PACMAN_RADIUS <= 0 || pacmanY + PACMAN_RADIUS >= SCREEN_HEIGHT) {
        currentState = STATE_GAMEOVER;
    }

    // 2. Update Pipes
    int pipesClearedCount = 0;

    for (int i = 0; i < cur.pipeCount; i++) {
        pipeX[i] -= cur.speed;

        // Level 2 Sine Wave
        if (currentLevel == 1) {
            float time = GetTime();
            pipeGapY[i] = initialPipeGapY[i] + sinf(time * 3.0f + i) * 50.0f;
        }

        // Collision Rectangles
        Rectangle topPipe = { pipeX[i], 0, PIPE_WIDTH, pipeGapY[i] };
        Rectangle botPipe = { pipeX[i], pipeGapY[i] + cur.gapSize, PIPE_WIDTH, SCREEN_HEIGHT };
        Rectangle player  = { PACMAN_X_POS - PACMAN_RADIUS + 5, pacmanY - PACMAN_RADIUS + 5, PACMAN_RADIUS*2 - 10, PACMAN_RADIUS*2 - 10 };

        if (CheckCollisionRecs(topPipe, player) || CheckCollisionRecs(botPipe, player)) {
            currentState = STATE_GAMEOVER;
        }

        // Orb Collection
        if (!orbCollected[i]) {
            Rectangle orbHitbox = {
                pipeX[i] + (PIPE_WIDTH / 2) - 5,
                pipeGapY[i] + orbRelY[i] - 5,
                10, 10
            };
            if (CheckCollisionRecs(player, orbHitbox)) {
                orbCollected[i] = true;
                currentSessionScore += 5;
            }
        }

        // Score Update (Passing Pipe)
        if (!pipePassed[i] && pipeX[i] + PIPE_WIDTH < PACMAN_X_POS) {
            pipePassed[i] = true;
            currentSessionScore += 1;
        }

        if (pipePassed[i]) pipesClearedCount++;
    }

    if (pipesClearedCount >= cur.pipeCount) {
        currentState = STATE_LEVEL_DONE;
    }
}

// ==========================================
//          DRAWING
// ==========================================

void DrawGame() {
    BeginDrawing();
    ClearBackground(BLACK);

    LevelData cur = levels[currentLevel];
    int border = 4; // Outline thickness

    if (currentState == STATE_INPUT) {
        DrawText("WELCOME TO FLAPPY PACMAN", 160, 100, 30, YELLOW);
        DrawText("Enter your name:", 300, 200, 20, WHITE);

        // Draw Input Box
        DrawRectangleLines(250, 230, 300, 40, WHITE);
        DrawText(tempName, 260, 240, 20, YELLOW);

        // Blinking cursor
        if ((int)(GetTime() * 2) % 2 == 0) {
            DrawText("_", 260 + MeasureText(tempName, 20), 240, 20, YELLOW);
        }

        DrawText("Press ENTER to Start", 280, 300, 20, DARKGRAY);
    }
    else if (currentState == STATE_VICTORY) {
        DrawText("YOU WIN!", 300, 50, 40, GOLD);
        DrawText("SCOREBOARD (Top 10)", 280, 120, 20, WHITE);
        DrawLine(280, 145, 520, 145, WHITE);

        for (int i = 0; i < playerCount; i++) {
            Color textColor = WHITE;
            // Highlight the current player's new score
            if (strcmp(players[i].name, tempName) == 0 && players[i].score == currentSessionScore) {
                textColor = YELLOW;
            }

            DrawText(TextFormat("%d. %s", i+1, players[i].name), 280, 160 + (i * 30), 20, textColor);
            DrawText(TextFormat("%d", players[i].score), 480, 160 + (i * 30), 20, textColor);
        }

        DrawText("Press SPACE to Play Again", 260, 420, 20, DARKGRAY);
    }
    else {
        // Draw Game Elements (Pipes, Orbs, Player)

        // 1. Pipes
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
                if (!orbCollected[i]) {
                     float finalOrbY = pipeGapY[i] + orbRelY[i];
                     DrawCircle(pipeX[i] + (PIPE_WIDTH/2), finalOrbY, 5, WHITE);
                }
            }
        }

        // 2. Pacman
        float tilt = pacmanVelocityY * 3.0f;
        if (tilt > 35.0f) tilt = 35.0f;
        if (tilt < -25.0f) tilt = -25.0f;

        DrawCircleSector((Vector2){PACMAN_X_POS, pacmanY}, PACMAN_RADIUS,
                        currentMouthAngle + tilt, (360.0f - currentMouthAngle) + tilt, 0, YELLOW);

        // 3. UI Overlays
        DrawText(TextFormat("Score: %d", currentSessionScore), 10, 10, 20, WHITE);
        DrawText(TextFormat("Level: %d", currentLevel + 1), 10, 35, 20, cur.color);

        if (currentState == STATE_GAMEOVER) {
            DrawText("GAME OVER", 280, 200, 40, RED);
            DrawText("Press SPACE to Retry Level", 260, 250, 20, WHITE);
        }
        else if (currentState == STATE_LEVEL_DONE) {
            DrawText("LEVEL COMPLETE!", 230, 200, 40, GREEN);
            DrawText("Press SPACE for Next Level", 260, 250, 20, WHITE);
        }
        else if (currentState == STATE_TITLE) {
            DrawText(TextFormat("LEVEL %d", currentLevel + 1), 340, 180, 30, cur.color);
            DrawText("Press SPACE to Fly", 300, 230, 20, WHITE);
        }
    }

    EndDrawing();
}

// ==========================================
//          MAIN
// ==========================================

int main(void) {
    srand(time(NULL));

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Flappy Pacman - Scoreboard Edition");
    SetTargetFPS(FPS);

    SetupLevels();

    // Initialize empty players
    for(int i=0; i<MAX_PLAYERS; i++) players[i].active = false;

    while (!WindowShouldClose()) {
        UpdateGame();
        DrawGame();
    }

    CloseWindow();
    return 0;
}
