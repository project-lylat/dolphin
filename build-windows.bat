@echo off

SET PATH=%PATH%;C:\Program Files\7-Zip;C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin;C:\Python27\;C:\Python27\Scripts;C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;C:\Windows\System32\WindowsPowerShell\v1.0\;C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;C:\Program Files\Microsoft VS Code\bin;C:\Program Files\Git\cmd

cd Source
msbuild.exe /v:m /p:Platform=x64 /p:Configuration=Release dolphin-emu.sln