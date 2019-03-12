@ECHO OFF

set TESS_BIN=bin

IF /I "%PROCESSOR_ARCHITECTURE%" == "amd64" (
    set TESS_BIN=bin64
)
IF /I "%PROCESSOR_ARCHITEW6432%" == "amd64" (
    set TESS_BIN=bin64
)

start %TESS_BIN%\tesseract.exe "-u$HOME\My Games\ValhallaProject" -gserver-log.txt -d %*
