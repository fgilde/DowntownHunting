@echo off
rem Builds a standalone Windows game into Build\Packaged\Windows (DowntownHunting.exe) and zips it with a short guide
rem into Build\DowntownHunting.zip, ready to send. Steam online is built in (Lyra's Config/Custom/Steam, test app id 480):
rem players need the Steam client running; offline and bot matches work without it.
rem Takes a long time on the first run (cooking City Sample content) and needs plenty of RAM.
rem City Sample content must be imported first (see docs\NOTES.md).
rem Clear the old archive first: stale files (an old exe, a dev fg.txt) would end up in the zip.
if exist "%~dp0Build\Packaged" rmdir /s /q "%~dp0Build\Packaged"
call "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun ^
  -project="%~dp0DowntownHunting.uproject" -target=DowntownHunting -platform=Win64 -clientconfig=Development ^
  -build -cook -stage -pak -iostore -archive -archivedirectory="%~dp0Build\Packaged" -nop4 -utf8output
if errorlevel 1 (echo Packaging failed. & pause & exit /b 1)
copy /y "%~dp0Tools\Package\LIESMICH.txt" "%~dp0Build\Packaged\Windows\LIESMICH.txt" >nul
del "%~dp0Build\DowntownHunting.zip" 2>nul
tar -a -c -f "%~dp0Build\DowntownHunting.zip" --exclude "*.pdb" --exclude fg.txt -C "%~dp0Build\Packaged" Windows
rem Dev mode for this PC only: the project fg.txt goes next to the packaged exe, never into the zip.
if exist "%~dp0fg.txt" copy /y "%~dp0fg.txt" "%~dp0Build\Packaged\Windows\fg.txt" >nul
echo Done: %~dp0Build\DowntownHunting.zip
if not "%1"=="nopause" pause
