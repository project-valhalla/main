@ECHO OFF

set VALHALLA_BIN=bin\bin32

IF /I "%PROCESSOR_ARCHITECTURE%" == "amd64" (
    set VALHALLA_BIN=bin\bin64
)
IF /I "%PROCESSOR_ARCHITEW6432%" == "amd64" (
    set VALHALLA_BIN=bin\bin64
)

start %VALHALLA_BIN%\valhalla.exe "-u$HOME\My Games\VALHALLA\v1.0.0-beta.2" -gserver-log.txt -d %*
