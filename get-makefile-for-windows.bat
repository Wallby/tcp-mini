@echo off

:: 1. clean old makefile if any
if exist Makefile del Makefile

:: 2. obtain latest makefile
git archive --format zip --output makefile-for-windows.zip makefile-for-windows

tar -xf makefile-for-windows.zip
del makefile-for-windows.zip