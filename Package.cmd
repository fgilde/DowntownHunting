@echo off
rem Builds a standalone Windows game into Build\Packaged\Windows (DowntownHunting.exe).
rem Takes a long time on the first run (cooking City Sample content) and needs plenty of RAM.
rem City Sample content must be imported first (see docs\NOTES.md).
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
  -project="%~dp0DowntownHunting.uproject" -target=LyraGame -platform=Win64 -clientconfig=Development ^
  -build -cook -stage -pak -iostore -archive -archivedirectory="%~dp0Build\Packaged" -nop4 -utf8output
if errorlevel 1 (echo Packaging failed. & pause & exit /b 1)
echo Done: %~dp0Build\Packaged\Windows\DowntownHunting.exe
pause
