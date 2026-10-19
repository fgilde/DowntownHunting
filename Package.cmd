@echo off
rem Builds a standalone Windows game into Build\Packaged\Windows (DowntownHunting.exe) and zips it with a short guide
rem into Build\DowntownHunting.zip, ready to send. Steam online is built in (Lyra's Config/Custom/Steam, test app id 480):
rem players need the Steam client running; offline and bot matches work without it.
rem Takes a long time on the first run (cooking City Sample content) and needs plenty of RAM.
rem City Sample content must be imported first (see docs\NOTES.md).
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
  -project="%~dp0DowntownHunting.uproject" -target=LyraGame -platform=Win64 -clientconfig=Development ^
  -build -cook -stage -pak -iostore -archive -archivedirectory="%~dp0Build\Packaged" -CustomConfig=Steam -nop4 -utf8output
if errorlevel 1 (echo Packaging failed. & pause & exit /b 1)
copy /y "%~dp0Tools\Package\LIESMICH.txt" "%~dp0Build\Packaged\Windows\LIESMICH.txt" >nul
del "%~dp0Build\DowntownHunting.zip" 2>nul
tar -a -c -f "%~dp0Build\DowntownHunting.zip" -C "%~dp0Build\Packaged" Windows
echo Done: %~dp0Build\DowntownHunting.zip
if not "%1"=="nopause" pause
