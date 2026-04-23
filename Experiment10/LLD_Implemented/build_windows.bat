@echo off
rem Build backend with MinGW g++ (requires -lws2_32)
g++ -std=c++17 backend\main.cpp -O2 -o backend\server.exe -lws2_32
if %errorlevel% neq 0 (
  echo Build failed
  exit /b %errorlevel%
)
echo Build succeeded: backend\server.exe
