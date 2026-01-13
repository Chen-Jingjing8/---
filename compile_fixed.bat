@echo off
chcp 65001 >nul
echo ========== 自动化编译脚本 ==========
echo 此脚本将编译src目录下的7个cpp文件
echo 生成DLL、SO和EXE文件到bin目录
echo.

REM 设置编译器和路径
set COMPILER=g++
set SRC_DIR=src
set BIN_DIR=bin

REM 检查编译器是否可用
%COMPILER% --version >nul 2>&1
if %errorlevel% neq 0 (
    echo 错误：未找到g++编译器，请确保已安装MinGW或MSYS2
    echo 或者将g++添加到系统PATH环境变量中
    pause
    exit /b 1
)

REM 创建bin目录（如果不存在）
if not exist %BIN_DIR% (
    mkdir %BIN_DIR%
    echo 创建目录: %BIN_DIR%
)

echo 开始编译...
echo.

REM 编译参数
set CXXFLAGS=-std=c++11 -O2 -Wall
set DLLFLAGS=-shared -fPIC
set EXEFLAGS=-static-libgcc -static-libstdc++

REM 创建简单的main文件
echo #include ^<stdio.h^> > main.cpp
echo #include ^<stdlib.h^> >> main.cpp
echo #include ^<string.h^> >> main.cpp
echo extern "C" { const char* simulateChargingStation(const char* input); } >> main.cpp
echo int main() { >> main.cpp
echo     printf("Drone Charging Station Test\n"); >> main.cpp
echo     printf("Input test command (e.g. a 1 0): "); >> main.cpp
echo     char input[100]; >> main.cpp
echo     fgets(input, sizeof(input), stdin); >> main.cpp
echo     input[strcspn(input, "\n")] = 0; >> main.cpp
echo     const char* result = simulateChargingStation(input); >> main.cpp
echo     printf("Result: %%s\n", result); >> main.cpp
echo     printf("Press any key to exit...\n"); >> main.cpp
echo     getchar(); >> main.cpp
echo     return 0; >> main.cpp
echo } >> main.cpp

REM 编译task1
echo 正在编译 task1.cpp...
echo   - 编译为Windows DLL (libtask1.dll)...
%COMPILER% %CXXFLAGS% %DLLFLAGS% -o %BIN_DIR%\libtask1.dll %SRC_DIR%\task1.cpp
if %errorlevel% neq 0 (
    echo    错误：编译libtask1.dll失败
    goto :error
)
echo   - 编译为Linux SO (libtask1.so)...
%COMPILER% %CXXFLAGS% %DLLFLAGS% -o %BIN_DIR%\libtask1.so %SRC_DIR%\task1.cpp
if %errorlevel% neq 0 (
    echo    错误：编译libtask1.so失败
    goto :error
)
echo   - 编译为可执行文件 (task1.exe)...
%COMPILER% %CXXFLAGS% %EXEFLAGS% -o %BIN_DIR%\task1.exe main.cpp %SRC_DIR%\task1.cpp
if %errorlevel% neq 0 (
    echo    错误：编译task1.exe失败
    goto :error
)
echo   task1.cpp 编译完成
echo.

REM 编译task2
echo 正在编译 task2.cpp...
echo   - 编译为Windows DLL (libtask2.dll)...
%COMPILER% %CXXFLAGS% %DLLFLAGS% -o %BIN_DIR%\libtask2.dll %SRC_DIR%\task2.cpp
if %errorlevel% neq 0 (
    echo    错误：编译libtask2.dll失败
    goto :error
)
echo   - 编译为Linux SO (libtask2.so)...
%COMPILER% %CXXFLAGS% %DLLFLAGS% -o %BIN_DIR%\libtask2.so %SRC_DIR%\task2.cpp
if %errorlevel% neq 0 (
    echo    错误：编译libtask2.so失败
    goto :error
)
echo   - 编译为可执行文件 (task2.exe)...
%COMPILER% %CXXFLAGS% %EXEFLAGS% -o %BIN_DIR%\task2.exe main.cpp %SRC_DIR%\task2.cpp
if %errorlevel% neq 0 (
    echo    错误：编译task2.exe失败
    goto :error
)
echo   task2.cpp 编译完成
echo.

REM 编译task3
echo 正在编译 task3.cpp...
echo   - 编译为Windows DLL (libtask3.dll)...
%COMPILER% %CXXFLAGS% %DLLFLAGS% -o %BIN_DIR%\libtask3.dll %SRC_DIR%\task3.cpp
if %errorlevel% neq 0 (
    echo    错误：编译libtask3.dll失败
    goto :error
)
echo   - 编译为Linux SO (libtask3.so)...
%COMPILER% %CXXFLAGS% %DLLFLAGS% -o %BIN_DIR%\libtask3.so %SRC_DIR%\task3.cpp
if %errorlevel% neq 0 (
    echo    错误：编译libtask3.so失败
    goto :error
)
echo   - 编译为可执行文件 (task3.exe)...
%COMPILER% %CXXFLAGS% %EXEFLAGS% -o %BIN_DIR%\task3.exe main.cpp %SRC_DIR%\task3.cpp
if %errorlevel% neq 0 (
    echo    错误：编译task3.exe失败
    goto :error
)
echo   task3.cpp 编译完成
echo.

REM 编译task4
echo 正在编译 task4.cpp...
echo   - 编译为Windows DLL (libtask4.dll)...
%COMPILER% %CXXFLAGS% %DLLFLAGS% -o %BIN_DIR%\libtask4.dll %SRC_DIR%\task4.cpp
if %errorlevel% neq 0 (
    echo    错误：编译libtask4.dll失败
    goto :error
)
echo   - 编译为Linux SO (libtask4.so)...
%COMPILER% %CXXFLAGS% %DLLFLAGS% -o %BIN_DIR%\libtask4.so %SRC_DIR%\task4.cpp
if %errorlevel% neq 0 (
    echo    错误：编译libtask4.so失败
    goto :error
)
echo   - 编译为可执行文件 (task4.exe)...
%COMPILER% %CXXFLAGS% %EXEFLAGS% -o %BIN_DIR%\task4.exe main.cpp %SRC_DIR%\task4.cpp
if %errorlevel% neq 0 (
    echo    错误：编译task4.exe失败
    goto :error
)
echo   task4.cpp 编译完成
echo.

REM 编译task5
echo 正在编译 task5.cpp...
echo   - 编译为Windows DLL (libtask5.dll)...
%COMPILER% %CXXFLAGS% %DLLFLAGS% -o %BIN_DIR%\libtask5.dll %SRC_DIR%\task5.cpp
if %errorlevel% neq 0 (
    echo    错误：编译libtask5.dll失败
    goto :error
)
echo   - 编译为Linux SO (libtask5.so)...
%COMPILER% %CXXFLAGS% %DLLFLAGS% -o %BIN_DIR%\libtask5.so %SRC_DIR%\task5.cpp
if %errorlevel% neq 0 (
    echo    错误：编译libtask5.so失败
    goto :error
)
echo   - 编译为可执行文件 (task5.exe)...
%COMPILER% %CXXFLAGS% %EXEFLAGS% -o %BIN_DIR%\task5.exe main.cpp %SRC_DIR%\task5.cpp
if %errorlevel% neq 0 (
    echo    错误：编译task5.exe失败
    goto :error
)
echo   task5.cpp 编译完成
echo.

REM 编译task6
echo 正在编译 task6.cpp...
echo   - 编译为Windows DLL (libtask6.dll)...
%COMPILER% %CXXFLAGS% %DLLFLAGS% -o %BIN_DIR%\libtask6.dll %SRC_DIR%\task6.cpp
if %errorlevel% neq 0 (
    echo    错误：编译libtask6.dll失败
    goto :error
)
echo   - 编译为Linux SO (libtask6.so)...
%COMPILER% %CXXFLAGS% %DLLFLAGS% -o %BIN_DIR%\libtask6.so %SRC_DIR%\task6.cpp
if %errorlevel% neq 0 (
    echo    错误：编译libtask6.so失败
    goto :error
)
echo   - 编译为可执行文件 (task6.exe)...
%COMPILER% %CXXFLAGS% %EXEFLAGS% -o %BIN_DIR%\task6.exe main.cpp %SRC_DIR%\task6.cpp
if %errorlevel% neq 0 (
    echo    错误：编译task6.exe失败
    goto :error
)
echo   task6.cpp 编译完成
echo.

REM 编译task7
echo 正在编译 task7.cpp...
echo   - 编译为Windows DLL (libtask7.dll)...
%COMPILER% %CXXFLAGS% %DLLFLAGS% -o %BIN_DIR%\libtask7.dll %SRC_DIR%\task7.cpp
if %errorlevel% neq 0 (
    echo    错误：编译libtask7.dll失败
    goto :error
)
echo   - 编译为Linux SO (libtask7.so)...
%COMPILER% %CXXFLAGS% %DLLFLAGS% -o %BIN_DIR%\libtask7.so %SRC_DIR%\task7.cpp
if %errorlevel% neq 0 (
    echo    错误：编译libtask7.so失败
    goto :error
)
echo   - 编译为可执行文件 (task7.exe)...
%COMPILER% %CXXFLAGS% %EXEFLAGS% -o %BIN_DIR%\task7.exe %SRC_DIR%\task7.cpp
if %errorlevel% neq 0 (
    echo    错误：编译task7.exe失败
    goto :error
)
echo   task7.cpp 编译完成
echo.

REM 清理临时文件
del main.cpp

echo ========== 编译完成 ==========
echo.
echo 生成的文件：
dir /b %BIN_DIR%\*.dll %BIN_DIR%\*.so %BIN_DIR%\*.exe 2>nul
echo.
echo 所有文件已成功编译到 %BIN_DIR% 目录
goto :end

:error
echo.
echo ========== 编译失败 ==========
echo 请检查错误信息并修复后重试
pause
exit /b 1

:end
echo.
echo 按任意键退出...
pause >nul
