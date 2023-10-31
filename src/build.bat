@echo off

cd ..\scripts

REM You can compile with either msvc or gcc
REM Just swap the scripts here and you are home

REM Enable line below to rebuild raylib
REM call msvc-build-raylib.bat 
call msvc-build.bat
cd ..\src
