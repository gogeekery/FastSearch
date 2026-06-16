@echo off
setlocal

set MINGW=C:\mingw64\bin
set CC=%MINGW%\gcc
set CXX=%MINGW%\g++

set OUT=Fastsearch.exe
set CPP_SRC=WinMain.cpp Regex.cpp Search.cpp Utils.cpp Editor.cpp
set RES=resource.o

set PCRE2_DIR=pcre2
set PCRE2_OBJ_DIR=build_pcre2_obj

if not exist "%PCRE2_OBJ_DIR%" mkdir "%PCRE2_OBJ_DIR%"

echo.
echo === Compiling PCRE2 C sources as 8-bit static objects ===
echo.

set PCRE2_DEFS=-DPCRE2_CODE_UNIT_WIDTH=8 -DPCRE2_STATIC -DHAVE_CONFIG_H
set PCRE2_FLAGS=-std=c11 -O3 -march=native -flto %PCRE2_DEFS% -I%PCRE2_DIR%

%CC% %PCRE2_FLAGS% -c %PCRE2_DIR%/pcre2_chkdint.c          -o %PCRE2_OBJ_DIR%/pcre2_chkdint.o
if errorlevel 1 goto fail

%CC% %PCRE2_FLAGS% -c %PCRE2_DIR%/pcre2_compile.c          -o %PCRE2_OBJ_DIR%/pcre2_compile.o
if errorlevel 1 goto fail

%CC% %PCRE2_FLAGS% -c %PCRE2_DIR%/pcre2_compile_class.c    -o %PCRE2_OBJ_DIR%/pcre2_compile_class.o
if errorlevel 1 goto fail

%CC% %PCRE2_FLAGS% -c %PCRE2_DIR%/pcre2_compile_cgroup.c   -o %PCRE2_OBJ_DIR%/pcre2_compile_cgroup.o
if errorlevel 1 goto fail

%CC% %PCRE2_FLAGS% -c %PCRE2_DIR%/pcre2_jit_compile.c      -o %PCRE2_OBJ_DIR%/pcre2_jit_compile.o
if errorlevel 1 goto fail

%CC% %PCRE2_FLAGS% -c %PCRE2_DIR%/pcre2_match.c            -o %PCRE2_OBJ_DIR%/pcre2_match.o
if errorlevel 1 goto fail

%CC% %PCRE2_FLAGS% -c %PCRE2_DIR%/pcre2_match_data.c       -o %PCRE2_OBJ_DIR%/pcre2_match_data.o
if errorlevel 1 goto fail

%CC% %PCRE2_FLAGS% -c %PCRE2_DIR%/pcre2_chartables.c       -o %PCRE2_OBJ_DIR%/pcre2_chartables.o
if errorlevel 1 goto fail

%CC% %PCRE2_FLAGS% -c %PCRE2_DIR%/pcre2_config.c           -o %PCRE2_OBJ_DIR%/pcre2_config.o
if errorlevel 1 goto fail

%CC% %PCRE2_FLAGS% -c %PCRE2_DIR%/pcre2_context.c          -o %PCRE2_OBJ_DIR%/pcre2_context.o
if errorlevel 1 goto fail

%CC% %PCRE2_FLAGS% -c %PCRE2_DIR%/pcre2_error.c            -o %PCRE2_OBJ_DIR%/pcre2_error.o
if errorlevel 1 goto fail

%CC% %PCRE2_FLAGS% -c %PCRE2_DIR%/pcre2_newline.c          -o %PCRE2_OBJ_DIR%/pcre2_newline.o
if errorlevel 1 goto fail

%CC% %PCRE2_FLAGS% -c %PCRE2_DIR%/pcre2_ord2utf.c          -o %PCRE2_OBJ_DIR%/pcre2_ord2utf.o
if errorlevel 1 goto fail

%CC% %PCRE2_FLAGS% -c %PCRE2_DIR%/pcre2_pattern_info.c     -o %PCRE2_OBJ_DIR%/pcre2_pattern_info.o
if errorlevel 1 goto fail

%CC% %PCRE2_FLAGS% -c %PCRE2_DIR%/pcre2_string_utils.c     -o %PCRE2_OBJ_DIR%/pcre2_string_utils.o
if errorlevel 1 goto fail

%CC% %PCRE2_FLAGS% -c %PCRE2_DIR%/pcre2_tables.c           -o %PCRE2_OBJ_DIR%/pcre2_tables.o
if errorlevel 1 goto fail

%CC% %PCRE2_FLAGS% -c %PCRE2_DIR%/pcre2_valid_utf.c        -o %PCRE2_OBJ_DIR%/pcre2_valid_utf.o
if errorlevel 1 goto fail

%CC% %PCRE2_FLAGS% -c %PCRE2_DIR%/pcre2_xclass.c           -o %PCRE2_OBJ_DIR%/pcre2_xclass.o
if errorlevel 1 goto fail

%CC% %PCRE2_FLAGS% -c %PCRE2_DIR%/pcre2_auto_possess.c     -o %PCRE2_OBJ_DIR%/pcre2_auto_possess.o
if errorlevel 1 goto fail

%CC% %PCRE2_FLAGS% -c %PCRE2_DIR%/pcre2_find_bracket.c     -o %PCRE2_OBJ_DIR%/pcre2_find_bracket.o
if errorlevel 1 goto fail

%CC% %PCRE2_FLAGS% -c %PCRE2_DIR%/pcre2_script_run.c       -o %PCRE2_OBJ_DIR%/pcre2_script_run.o
if errorlevel 1 goto fail

%CC% %PCRE2_FLAGS% -c %PCRE2_DIR%/pcre2_extuni.c           -o %PCRE2_OBJ_DIR%/pcre2_extuni.o
if errorlevel 1 goto fail

%CC% %PCRE2_FLAGS% -c %PCRE2_DIR%/pcre2_ucd.c              -o %PCRE2_OBJ_DIR%/pcre2_ucd.o
if errorlevel 1 goto fail

%CC% %PCRE2_FLAGS% -c %PCRE2_DIR%/pcre2_study.c            -o %PCRE2_OBJ_DIR%/pcre2_study.o
if errorlevel 1 goto fail


echo.
echo === Linking C++ application ===
echo.

%CXX% ^
-std=c++17 -O3 -march=native -flto -mavx2 -municode -mwindows -s ^
-DUSE_PCRE2 -DPCRE2_CODE_UNIT_WIDTH=8 -DPCRE2_STATIC -DHAVE_CONFIG_H ^
-I%PCRE2_DIR% ^
%CPP_SRC% ^
%RES% ^
%PCRE2_OBJ_DIR%/pcre2_chkdint.o ^
%PCRE2_OBJ_DIR%/pcre2_compile.o ^
%PCRE2_OBJ_DIR%/pcre2_compile_class.o ^
%PCRE2_OBJ_DIR%/pcre2_compile_cgroup.o ^
%PCRE2_OBJ_DIR%/pcre2_jit_compile.o ^
%PCRE2_OBJ_DIR%/pcre2_match.o ^
%PCRE2_OBJ_DIR%/pcre2_match_data.o ^
%PCRE2_OBJ_DIR%/pcre2_chartables.o ^
%PCRE2_OBJ_DIR%/pcre2_config.o ^
%PCRE2_OBJ_DIR%/pcre2_context.o ^
%PCRE2_OBJ_DIR%/pcre2_error.o ^
%PCRE2_OBJ_DIR%/pcre2_newline.o ^
%PCRE2_OBJ_DIR%/pcre2_ord2utf.o ^
%PCRE2_OBJ_DIR%/pcre2_pattern_info.o ^
%PCRE2_OBJ_DIR%/pcre2_string_utils.o ^
%PCRE2_OBJ_DIR%/pcre2_tables.o ^
%PCRE2_OBJ_DIR%/pcre2_valid_utf.o ^
%PCRE2_OBJ_DIR%/pcre2_xclass.o ^
%PCRE2_OBJ_DIR%/pcre2_auto_possess.o ^
%PCRE2_OBJ_DIR%/pcre2_find_bracket.o ^
%PCRE2_OBJ_DIR%/pcre2_script_run.o ^
%PCRE2_OBJ_DIR%/pcre2_extuni.o ^
%PCRE2_OBJ_DIR%/pcre2_ucd.o ^
%PCRE2_OBJ_DIR%/pcre2_study.o ^
-o %OUT% ^
-static -static-libgcc -static-libstdc++ ^
-lcomctl32 -lole32 -luxtheme -lshlwapi -ldwmapi -lshell32 -luser32 -lgdi32

if errorlevel 1 goto fail

echo.
echo Build complete: %OUT%
echo.
pause
exit /b 0

:fail
echo.
echo Build failed.
echo.
pause
exit /b 1
