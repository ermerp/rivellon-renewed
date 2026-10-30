@echo off
rem Rivellon Renewed setup: double-click in the Divine Divinity folder. Installs dgVoodoo2 or
rem uninstalls the mod (RivellonRenewed\setup.ps1 does the work, visibly in this window).
title Rivellon Renewed setup
if not exist "%~dp0RivellonRenewed\setup.ps1" (
    echo RivellonRenewed\setup.ps1 not found next to this file.
    echo Unpack the whole Rivellon Renewed zip into the Divine Divinity folder.
    pause
    exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0RivellonRenewed\setup.ps1" %*
rem Exit code 10: the mod was uninstalled - remove this file as well.
if errorlevel 10 (goto) 2>nul & del "%~f0"
