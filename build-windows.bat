@echo off
rem Release x64 build with CMake and Ninja (upstream removed the Visual Studio solution).
rem Run from an "x64 Native Tools Command Prompt for VS 2022".
rem The program ends up in Build\x64\Binaries.

cmake -S . -B Build\x64 -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
cmake --build Build\x64 || exit /b 1
