@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /EHsc /utf-8 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /I F:\Projects\Mods\MMORPGTycoon2\include /I F:\Projects\Mods\MMORPGTycoon2\include\vendor\steam /FoF:/Projects/Mods/MMORPGTycoon2/test/session_liveness_evidence/ /FeF:\Projects\Mods\MMORPGTycoon2\test\session_liveness_evidence\session_liveness_fixture.exe F:\Projects\Mods\MMORPGTycoon2\test\session_liveness_fixture.cpp F:\Projects\Mods\MMORPGTycoon2\src\net\sidecar_client.cpp F:\Projects\Mods\MMORPGTycoon2\src\net\local_session.cpp /link ws2_32.lib
exit /b %errorlevel%
