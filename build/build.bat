@echo off
robocopy libs build\sysroot\usr\local\lib\flaxlibs /e /nfl /ndl /njh /njs /nc /ns /np
echo Compiling...

SETLOCAL

call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"

IF /I "%1"=="release" (
	SET buildDir="build\meson-reldbg"
) ELSE (
	SET buildDir="build\meson-dbg"
)

ninja -C %buildDir%

ENDLOCAL