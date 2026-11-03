@echo off
rem Removes Skullgirls Fixes by putting the game's own Tolk.dll back. Logs stay in sgfixes-logs.
cd /d "%~dp0.."
if not exist Tolk_orig.dll (echo Nothing to restore: Tolk_orig.dll isn't in the game folder. & goto end)
move /y Tolk_orig.dll Tolk.dll >nul && echo Removed Skullgirls Fixes. You can now delete the sgfixes folder. && goto end
echo Uninstall failed. Close the game and try again.
:end
pause
