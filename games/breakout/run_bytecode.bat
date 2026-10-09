@echo off
cd /d "%~dp0\..\.."
vesbo.exe -c games\breakout\breakout.vsb -o games\breakout\breakout.vbo
vesbo.exe games\breakout\breakout.vbo
