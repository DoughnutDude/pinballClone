@echo off

set LIBRARIES=kernel32.lib user32.lib shell32.lib winmm.lib gdi32.lib opengl32.lib
set RAYLIB=rcore.c rmodels.c raudio.c rglfw.c rshapes.c rtext.c rtextures.c utils.c -I.\external\glfw\include
set RAYLIB_DEFINES=/DBUILD_LIBTYPE_SHARED /DPLATFORM_DESKTOP

call msvc_upgrade_cmd_64.bat

cd ..
mkdir bin
cd src\raylib\src

cl /LD %RAYLIB_DEFINES%  /Fe: "raylib" %RAYLIB% /link %LIBRARIES%

copy raylib.h        ..
REM copy rmem.h          ..
REM copy physac.h        ..
copy raymath.h       ..

copy raylib.dll ..\..\..\bin
copy raylib.lib ..\..\..\bin

cd ..\..\..\scripts
