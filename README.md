# Pinball Clone

## Description:

Very WIP.

Just a pinball clone using raylib made primarily for educational purposes. Added hot loaded code for easier debugging and possibility for similar game engine features. Only runs on Windows for now.

## Controls:

TBD

## How to run:

You need the gcc compiler (which is packaged with raylib installation) 

1. Run build.bat
2. Run run.bat 
3. Done

If you want to use msvc: just download Visual Studio Community 2019 and it should work just replace gcc calls with msvc calls in build.bat and run build.bat script, If you have any trouble with that checkout Casey Muratori's intro to Windows programming https://hero.handmade.network/episode/code/day001/ the compiler needs to be on your system path so that the system can find it.

## Structure

* executable_main.c -> Loads the dynamic library(hotloaded_main.c) and calls Update from it in a loop. On every iteration checks if the dynamic library changed, if it did then it swaps the code 
* hotloaded_main.c -> Gamecode(dynamic library), this is where you actually write the engine, game etc. It has Update function which is called on every frame, it also has Initialize function which is called on program start, HotReload function which is called when you recompile the program while its running and the code gets swapped
* shared.h -> code shared between the executable and the dynamic library 

## Code Idea Sources

https://github.com/krzosa/raylib_hot_reload_template
https://hero.handmade.network/episode/code/day021/
https://github.com/raysan5/raylib
https://www.youtube.com/watch?v=6rgiPrzqt9w
