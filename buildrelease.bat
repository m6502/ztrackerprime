@echo off
:: /*************************************************************************
::  *
::  * FILE  buildrelease.bat
::  * $Id: buildrelease.bat,v 1.19 2002/04/15 03:06:30 zonaj Exp $
::  *
::  * DESCRIPTION 
::  *   Build the binary release archive.
::  *   This assumes that zt, ztconfig and ztskin was already built as
::  *   release.
::  *
::  ******/

set ARCHIVE=zt-0.97.zip

:: /* all .dll's are included in the required_libs dir */
set ZLIB=required_libs
set SDL=required_libs
set LIBPNG=required_libs

:: /* for now this must be relative to the current directory, as I couldn't
::    find out how to set a variable to the current directory path. /tlr */
set TMPREL=ztracker_temp
set PWD=..

:: /* build the release directory structure */
mkdir %TMPREL%
upx -9 release\zt.exe -o %TMPREL%\zt.exe
upx -9 release\ztconf.exe -o %TMPREL%\ztconf.exe
upx -9 release\ztskin.exe -o %TMPREL%\ztskin.exe
copy zt.conf %TMPREL%
copy LICENSE.txt %TMPREL%
copy README-SDL.txt %TMPREL%
copy docs\help.txt %TMPREL%
mkdir %TMPREL%\skins
mkdir %TMPREL%\skins\default
copy skins\default\*.png %TMPREL%\skins\default
copy skins\default\font.fnt %TMPREL%\skins\default
copy skins\default\colors.conf %TMPREL%\skins\default
mkdir %TMPREL%\skins\professional
copy skins\professional\*.png %TMPREL%\skins\professional
copy skins\professional\font.fnt %TMPREL%\skins\professional
copy skins\professional\colors.conf %TMPREL%\skins\professional
mkdir %TMPREL%\docs
copy docs\README-NOW.txt %TMPREL%\docs
copy docs\CHANGELOG.txt %TMPREL%\docs
copy docs\zt-manual.pdf %TMPREL%\docs
copy %ZLIB%\zlib.dll %TMPREL%
copy %SDL%\SDL.dll %TMPREL%
copy %LIBPNG%\libpng2.dll %TMPREL%

:: /* build the zip-archive and clean up */
cd %TMPREL%
zip -9 -r %ARCHIVE% *
cd %PWD%
copy %TMPREL%\%ARCHIVE% .
deltree /y %TMPREL% 
:: /* eof */
