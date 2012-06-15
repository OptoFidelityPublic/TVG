set MYDIR=%~dp0
set ROOTDIR=%MYDIR%..

for /f %%a in ('git describe --always') do set VERSION=%%a
set PKGDIR=%MYDIR%tvg-%VERSION%
rmdir /S /Q "%PKGDIR%"
mkdir "%PKGDIR%"
mkdir "%PKGDIR%\gstreamer"

xcopy /s "%ROOTDIR%\gstreamer-gpl\*" "%PKGDIR%\gstreamer"
copy "%ROOTDIR%\doc\tvg_manual.pdf" "%PKGDIR%\"
copy "%ROOTDIR%\GstOFTVG\Release\GstOFTVG.dll" "%PKGDIR%\gstreamer\bin\plugins\"

mkdir "%PKGDIR%\debug"
mkdir "%PKGDIR%\bin"
copy "%MYDIR%gst-launch.bat" "%PKGDIR%\bin"

copy "%ROOTDIR%\doc\Run_TVG.bat" "%PKGDIR%\"
copy "%ROOTDIR%\examples\big_buck_bunny_1080p_h264.mov" "%PKGDIR%\"
copy "%ROOTDIR%\examples\layout.bmp" "%PKGDIR%\"
copy "%ROOTDIR%\examples\iPod.tvg" "%PKGDIR%\"
copy "%ROOTDIR%\examples\layout_vertical.bmp" "%PKGDIR%\"
