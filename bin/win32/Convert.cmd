@echo off
echo Converts all files in current folder.
pause

forfiles /m *.bin /c "sconvert.exe sconvert.exe @file"
