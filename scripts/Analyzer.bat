@echo OFF

:: Analyze a generated test video file.
:: Usage: Analyzer.sh <videofile>

call %~dp0gstreamer\env.bat

tvg_analyzer "%1"

if not [%2]==[nopause] (
@echo Done! Press enter to exit.
PAUSE
)

