:: Windows build script for PEel shared library
:: x8esix

@echo off

:: Constructs resource file for PEel.so
:: Windows 7A Sdk and MSVS 2010 required


set PATH=%PATH%;C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\Bin;C:\Program Files\Microsoft Visual Studio 10.0\VC\Bin;C:\Program Files\Microsoft Visual Studio 10.0\Common7\ID

set INCLUDE=%INCLUDE%;C:\Program Files\Microsoft SDKs\Windows\v7.0A\Include;C:\Program Files\Microsoft Visual Studio 10.0\VC\include;C:\Program Files\Microsoft Visual Studio 10.0\VC\atlmfc\include

set LIB=%LIB%;C:\Program Files\Microsoft SDKs\Windows\v7.0A\Lib;C:\Program Files\Microsoft Visual Studio 10.0\VC\lib;


@echo off

set objects=

:: cleanup     
     FOR %%i in (*.o) DO del %%i
     IF EXIST ..\Release\PEel32.dll del ..\Release\PEel32.dll /Q

cd .\peel\

     FOR %%i in (*.c) DO (gcc -c -DBUILDING_EXAMPLE_DLL -DSUPPORT_PE32 %%i -std=c99 -Os -s -pedantic & ECHO Compiling %%i)

     ECHO.
     ECHO Linking library...
     ECHO.

     FOR %%i in (*.o) DO (call :concat2 %%i & ECHO Adding %%i to library)    
  ::   windres peel.rc -O coff -o peel.res
  ::   call :concat2 peel.res

     dllwrap --def ..\doc\PEel32.def -o ..\Release\PEel32.dll %objects%
     
   :: gcc -shared -o ..\Release\PEel32.dll %objects% -Wl,--out-implib,..\Release\PEel_dll32.lib

ECHO.
ECHO Cleaning up...
ECHO.
    
     FOR %%i in (*.o) DO (del %%i & ECHO Deleting %%i...)

ECHO.
ECHO Compilation complete!
ECHO.
pause

:concat2:
set objects=%objects% %1
goto :eof