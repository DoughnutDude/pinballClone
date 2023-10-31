#include "shared.h"
//#include "stdio.h"

// Called on every frame
void Update(GameState *gameState)
{
    UpdateMusicStream(gameState->music);
    if (IsKeyPressed(KEY_SPACE))
    {
        PlaySound(gameState->fxCoin);
    }

    if ((IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) && (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)))
    {
        int monitor = GetCurrentMonitor();
        if (IsWindowFullscreen())
        {
            ClearWindowState(FLAG_WINDOW_UNDECORATED);
            ToggleFullscreen();
            SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
        } else
        {
            SetWindowState(FLAG_WINDOW_UNDECORATED);
            SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
            ToggleFullscreen();
        }
    }

    BeginDrawing();
    {
        ClearBackground(DARKGRAY);
        DrawRectangle(600, 100, 100, 100, DARKBROWN);
    }
    EndDrawing();
}

// Called when you recompile the program while its running
void HotReload(GameState *gameState)
{
    TraceLog(LOG_INFO, "HOTRELOAD");

    // Load global data (assets that must be available in all screens, i.e. font)
    ChangeDirectory(GetWorkingDirectory());
    //OutputDebugStringA(GetWorkingDirectory());//debug output
    ChangeDirectory("../src");
    gameState->font = LoadFont("resources/Inconsolata-ExtraBold.ttf");
    gameState->music = LoadMusicStream("resources/ambient.ogg");
    gameState->fxCoin = LoadSound("resources/coin.wav");
    Image image = LoadImage("resources/WIND.png");
    SetWindowIcon(image);
    SetWindowOpacity(0.9f);
    ChangeDirectory(GetWorkingDirectory());

    SetMusicVolume(gameState->music, 1.0f);
    //PlayMusicStream(gameState->music);
}

// Called before the dynamic libraries get swapped
void HotUnload(GameState *gameState)
{
    TraceLog(LOG_INFO, "HOTUNLOAD");

    StopMusicStream(gameState->music);

    UnloadFont(gameState->font);
    UnloadMusicStream(gameState->music);
    UnloadSound(gameState->fxCoin);

}

// Called at the the start of the program
void Initialize(GameState *gameState)
{
    InitWindow(1280, 720, "pinballClone");
    SetExitKey(KEY_KP_0);

    InitAudioDevice();      // Initialize audio device

    SetTargetFPS(60);

    HotReload(gameState);
}
