@echo off
setlocal
cd /d "%~dp0"

echo Deploying GrokLink landing (Worker assets) ...
call npx.cmd wrangler deploy
if errorlevel 1 exit /b 1

echo.
echo Deploy complete.
echo Site:    https://groklink.jonbailey.xyz/
echo Preview: https://groklink.pitchfork-and-torch.workers.dev/
echo Repo:    https://github.com/Pitchfork-and-Torch/GrokLink-OS
endlocal
