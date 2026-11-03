@echo off
rem Installs Skullgirls Fixes from the sgfixes folder, keeping the game's Tolk.dll as Tolk_orig.dll.
cd /d "%~dp0.."
if not exist SkullGirls.exe (echo The sgfixes folder must be in the game folder, next to SkullGirls.exe. & goto end)
if not exist Tolk_orig.dll (move Tolk.dll Tolk_orig.dll >nul || goto failed)
copy /y sgfixes\Tolk.dll Tolk.dll >nul && echo Installed Skullgirls Fixes. && goto end
:failed
echo Install failed. Close the game and try again.
:end
pause
