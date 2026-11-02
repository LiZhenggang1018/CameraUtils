@echo off
rem 恢复系统默认代码页（中文 = 936），与 PowerShell 脚本内部编码保持一致
chcp 936 >nul
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0list.ps1"