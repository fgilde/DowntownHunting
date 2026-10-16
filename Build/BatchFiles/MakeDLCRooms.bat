@ECHO OFF
cls

IF "%1"=="" GOTO usage
IF "%2"=="" GOTO usage

SET PLATFORM=%1
SET ARCHIVE_DIR=%2
SET EXTRA_ARGS=%3 %4 %5 %6 %7 %8 %9

ECHO *** Creating DLC packages for %PLATFORM% in %ARCHIVE_DIR%. Note that not all platforms support DLC package creation ***
ECHO.
ECHO.

REM change to root directory
SET ORIGINAL_DIR=%CD%
PUSHD "%~dp0..\..\..\..\..\"

REM base game must be built as a release version so DLC can reference it
ECHO *** Building Base Game ***
ECHO.
ECHO.
CALL .\Engine\Build\BatchFiles\RunUAT.bat BuildCookRun -nop4 -project=./Samples/Games/Lyra/Lyra.uproject -target=LyraGame -platform=%PLATFORM% -package -pak -build -cook -stage -createreleaseversion=BaseGame -archive -archivedirectory=%ARCHIVE_DIR%\BaseGame %EXTRA_ARGS%

ECHO.
ECHO.
ECHO *** Building DLC: RedRoom ***
ECHO.
ECHO.
CALL .\Engine\Build\BatchFiles\RunUAT.bat BuildCookRun -nop4 -project=./Samples/Games/Lyra/Lyra.uproject -target=LyraGame -platform=%PLATFORM% -package -pak -build -cook -stage -basedonreleaseversion=BaseGame -dlcname=RedRoom -archive -archivedirectory=%ARCHIVE_DIR%\RedRoom %EXTRA_ARGS%

ECHO.
ECHO.
ECHO *** Building DLC: GreenRoom ***
ECHO.
ECHO.
CALL .\Engine\Build\BatchFiles\RunUAT.bat BuildCookRun -nop4 -project=./Samples/Games/Lyra/Lyra.uproject -target=LyraGame -platform=%PLATFORM% -package -pak -build -cook -stage -basedonreleaseversion=BaseGame -dlcname=GreenRoom -archive -archivedirectory=%ARCHIVE_DIR%\GreenRoom %EXTRA_ARGS%

REM log out where to find the packages, and open the folder too
IF EXIST "%ARCHIVE_DIR%" (
    ECHO.
    ECHO.
    ECHO *** DLC build complete: %ARCHIVE_DIR% ***
    ECHO.
    ECHO.
    explorer "%ARCHIVE_DIR%"
) ELSE (
    ECHO.
    ECHO.
    ECHO *** DLC build FAILED - output folder not found: %ARCHIVE_DIR% ***
    ECHO.
    ECHO.
)

REM restoring the original directory this way because RunUAT doesn't always honor pushd/popd
CD /D "%ORIGINAL_DIR%"
GOTO :EOF

:usage
ECHO.
ECHO Usage: MakeDLCRooms.bat ^<platform^> ^<archivedirectory^>
ECHO.
ECHO   platform         e.g. Win64
ECHO   archivedirectory e.g. D:\Packages\Lyra
ECHO.
