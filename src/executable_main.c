#include "shared.h"
#include "win32_hot_reload.c"
#define PATH_SIZE 2048

// NOTE: This file should be cross-compatible, one thing you need to provide
// if you want to do a linux version of this is providing a "Sleep(time)" function

int main(int argumentCount, char *argumentArray[])
{
    // NOTE: first argument of the argumentArray is the relative path
    //      to the executable
    const char *basePath = GetDirectoryPath(argumentArray[0]);
    char mainDllPath[PATH_SIZE];
    char tempDllPath[PATH_SIZE];
    char lockFilePath[PATH_SIZE];

    // NOTE: build paths to our runtime library and the lockfile
    {
        int bytesCopied;
        bytesCopied = TextCopy(mainDllPath, basePath);
        TextAppend(mainDllPath, "/pinballClone_code.dll", &bytesCopied);
        bytesCopied = TextCopy(tempDllPath, basePath);
        TextAppend(tempDllPath, "/pinballClone_code_temp.dll", &bytesCopied);
        bytesCopied = TextCopy(lockFilePath, basePath);
        TextAppend(lockFilePath, "/lock.file", &bytesCopied);

        TraceLog(LOG_INFO, basePath);
        TraceLog(LOG_INFO, mainDllPath);
        TraceLog(LOG_INFO, tempDllPath);
        TraceLog(LOG_INFO, lockFilePath);
    }

    GameCode gameCode = {0};
    gameCode = GameCodeLoad(mainDllPath, tempDllPath, lockFilePath);

    GameState previousState = {0};
    GameState gameState = {0};
    gameCode.initialize(&gameState);

    double t = 0;
    double dt = 1.0/60.0;

    double currentTime = GetTime();
    double accumulator = 0;

    while(!WindowShouldClose())
    {
        // NOTE: Check if the code got recompiled
        long dllFileWriteTime = GetFileModTime(mainDllPath);
        if (dllFileWriteTime != gameCode.lastDllWriteTime)
        {
            gameCode.hotUnload(&gameState);
            GameCodeUnload(&gameCode);
            gameCode = GameCodeLoad(mainDllPath, tempDllPath, lockFilePath);
            gameCode.hotReload(&gameState);
        }

        double newTime = GetTime();
        double frameTime = newTime - currentTime;
        if (frameTime > 0.25)
        {
            frameTime = 0.25;
        }
        currentTime = newTime;
        
        accumulator += frameTime;
        
        while (accumulator >= dt)
        {
            previousState = gameState;
            gameState.dt = dt;
            gameCode.update(&gameState);
            t += dt;
            accumulator -= dt;
        }

        double alpha = accumulator / dt;

        // This is the one that gets rendered
        //GameState state = gameState * alpha + previousState * (1.0 - alpha);

        //gameState.dt = (float)(GetTime() - currentTime);
        //currentTime = GetTime();
    }

    //StopMusicStream(gameState.music);

    UnloadFont(gameState.font);
    UnloadMusicStream(gameState.music);
    UnloadSound(gameState.fxCoin);

    CloseAudioDevice();     // Close audio context
    CloseWindow();

    return 0;
}
