@echo off
rem Build wrapper for the x64-debug preset.
rem
rem Fixes ninja's header-dependency tracking on Chinese Windows:
rem   MSVC /showIncludes prints "note: including file:" localized, and ninja's
rem   msvc_deps_prefix (UTF-8) only matches when cl.exe emits UTF-8. Setting the
rem   console code page to 65001 makes cl.exe emit UTF-8, so ninja parses the
rem   real #deps (651 for TestMain.cpp.obj) instead of "#deps 0".
rem
rem Usage:
rem   scripts\build-debug.bat [target]
rem
rem Examples:
rem   scripts\build-debug.bat                       (build default targets)
rem   scripts\build-debug.bat camera_utils_tests    (build a specific target)

chcp 65001 >nul
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

if "%~1"=="" (
    cmake --build --preset x64-debug
) else (
    cmake --build --preset x64-debug --target %*
)
