
#include "raylib.h"

int main() {
    InitWindow(800, 450, "PlayTrack - Build Test");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(Color{24, 26, 35, 255});
        DrawText("PlayTrack", 320, 170, 40, RAYWHITE);
        DrawText("Raylib build test successful!", 265, 230, 20, GREEN);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
