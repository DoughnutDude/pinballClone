#include "raylib/raylib.h"

#define local_persist static
#define global_var	  static
#define internal      static

typedef struct GameState
{
    Font font;
    Music music;
    Sound fxCoin;
} GameState;