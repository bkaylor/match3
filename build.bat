@pushd bin
cl /Wall ..\src\main.c /Fematch3.exe /Zi /I..\msvc_sdl\SDL2-2.0.9\include /I..\msvc_sdl\SDL2_ttf-2.0.15\include /I..\msvc_sdl\SDL2_image-2.0.4\include /link /LIBPATH:..\msvc_sdl\SDL2-2.0.9\lib\x64 /LIBPATH:..\msvc_sdl\SDL2_ttf-2.0.15\lib\x64 /LIBPATH:..\msvc_sdl\SDL2_image-2.0.4\lib\x64 /SUBSYSTEM:CONSOLE "SDL2_ttf.lib" "SDL2_image.lib" "SDL2main.lib" "SDL2.lib"
@popd

:@pushd bin 
:gcc -Wall -g -std=c99 -I"../include" -c ../src/main.c
:gcc main.o -L"../lib" -Wl,-subsystem,windows -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf -:lSDL2_image -o chess
:@popd
