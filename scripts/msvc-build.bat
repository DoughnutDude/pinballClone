@echo off

set LINKER_FLAGS=/link raylib.lib kernel32.lib user32.lib shell32.lib winmm.lib gdi32.lib opengl32.lib -incremental:no -opt:ref
set EXPORTED_FUNCTIONS=/EXPORT:Update /EXPORT:Initialize /EXPORT:HotReload /EXPORT:HotUnload
set COMMON_FLAGS=/Zi /nologo /TP
set EXE_NAME=pinballClone.exe
set DLL_NAME=pinballClone_code.dll
set PDB_NAME=pinballClone_code_%random%.pdb

call msvc_upgrade_cmd_64.bat
cd ..\bin

del *.pdb > NUL 2> NUL
echo LOCKFILE IN AID OF HOTLOADING > lock.file
cl ..\src\hotloaded_main.c /LD /Fe:%DLL_NAME% %COMMON_FLAGS% %LINKER_FLAGS% -PDB:%PDB_NAME% %EXPORTED_FUNCTIONS%
del lock.file
cl ..\src\executable_main.c /D_AMD64_ /Fe:%EXE_NAME% %COMMON_FLAGS% %LINKER_FLAGS%

cd ..\scripts

REM Comments
REM /LD   - create a dll file, dynamic library
REM /Zi   - generate debugger files
REM /Fe   - change file name
REM -incremental:no -opt:ref - https://docs.microsoft.com/en-us/cpp/build/reference/incremental-link-incrementally?view=vs-2019
REM -D_AMD64_ - define a _AMD64_ macro, wouldnt work without this
