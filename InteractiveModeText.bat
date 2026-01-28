@echo off
cd /d %~dp0
DSConv.exe -i "int MyValues[10] = {99,88,77,66,55,44,33,22,11,}; int x2y[2] = {1001, 0110};" -o InteractiveModeTest.txt
pause