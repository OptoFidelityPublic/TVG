:: Edit this file to select the file formats to use,
:: and then run it to generate the video.

:: Name of input file (any supported video format)
SET INPUT=big_buck_bunny_1080p_h264.mov

:: Name of layout file (bitmap image defining the marker locations)
SET LAYOUT=layout.bmp

:: Name of output file
SET OUTPUT=output.mp4

:: Output format (select one of the examples or write your own combination)
SET FORMAT=x264enc ! mp4mux

:: Test video generator options
:: Number of frames to process (-1 to determine by the number of frame id markers)
SET NUM_BUFFERS=256

:: Number of times to repeat the test video
SET REPEAT=2

@echo "Starting test video generator.."

:: Actual command that executes gst-launch
gst-launch --gst-debug=progressreport0:3 --gst-plugin-load=GstOFTVG.dll ^
	filesrc location=%INPUT% ! decodebin2 ! queue ^
	! oftvg location=%LAYOUT% num-buffers=%NUM_BUFFERS% repeat=%REPEAT% silent=0 ^
	! progressreport update-freq=1 ! queue ^
	! ffmpegcolorspace ! %FORMAT% ! filesink location=%OUTPUT%

@echo "Done! Press enter to exit."
PAUSE