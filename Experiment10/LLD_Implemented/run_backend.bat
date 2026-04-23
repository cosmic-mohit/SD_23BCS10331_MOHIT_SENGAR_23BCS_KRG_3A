@echo off
if exist backend\server.exe (
  backend\server.exe
) else (
  echo backend\server.exe not found. Build first with build_windows.bat or compile in WSL.
)
