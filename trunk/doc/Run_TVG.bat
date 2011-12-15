:: Edit this file to select the file formats to use,
:: and then run it to generate the video.

:: Name of input file (any supported video format)
SET INPUT=big_buck_bunny_1080p_h264.mov

:: Name of layout file (bitmap image defining the marker locations)
SET LAYOUT=layout.bmp

:: Name of output file
SET OUTPUT=output.3gp

:: Video compression (select one)
:: You can get configuration parameters for each format by running gst-inspect x264enc
:: - x264enc      H.264 video
:: - ffenc_mjpeg  MJPEG video
:: - ffenc_mpeg4  MPEG-4 part 2
:: - ffenc_wmv2   Windows Media Video 8
:: - ffenc_flv    Flash video 
SET COMPRESSION=x264enc speed-preset=4

:: Video container format (select one)
:: - mp4mux       .MP4
:: - gppmux       .3GP
:: - avimux       .AVI
:: - qtmux        .MOV
:: - asfmux       .ASF
SET CONTAINER=gppmux

:: Preprocessing (you can combine the following by writing ! in between)
:: - videoscale ! video/x-raw-yuv,width=XXX,height=XXX    Resize the video
:: - videorate ! video/x-raw-yuv,framerate=XXX/1          Lower FPS by dropping frames
::SET PREPROCESS=! videoscale ! video/x-raw-yuv,width=640,height=480
::SET PREPROCESS=! videorate ! video/x-raw-yuv,framerate=10/1

:: Test video generator options
:: Number of frames to process (-1 to determine by the number of frame id markers)
SET NUM_BUFFERS=256

:: Number of times to repeat the test video
SET REPEAT=5

@echo "Starting test video generator.."

:: Actual command that executes gst-launch
gst-launch --gst-plugin-load=GstOFTVG.dll ^
	filesrc location=%INPUT% ! decodebin2 %PREPROCESS% ! queue ^
	! oftvg location=%LAYOUT% num-buffers=%NUM_BUFFERS% repeat=%REPEAT% silent=1 ^
	! queue ! ffmpegcolorspace ! %COMPRESSION% ! %CONTAINER% ! filesink location=%OUTPUT%

@echo "Done! Press enter to exit."
PAUSE