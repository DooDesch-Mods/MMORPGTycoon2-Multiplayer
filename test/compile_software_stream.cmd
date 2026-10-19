@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /EHsc /W4 /Iinclude test\software_stream_fixture.cpp /Fotest\software_stream_evidence\ /Fetest\software_stream_evidence\fixture.exe /link /INCREMENTAL:NO
if errorlevel 1 exit /b %errorlevel%
test\software_stream_evidence\fixture.exe
exit /b %errorlevel%
