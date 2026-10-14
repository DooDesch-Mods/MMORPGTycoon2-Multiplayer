@echo off
python "%~dp0tools\stop_local_test.py" %*
set "mt2mp_result=%errorlevel%"
if not "%mt2mp_result%"=="0" pause
exit /b %mt2mp_result%
