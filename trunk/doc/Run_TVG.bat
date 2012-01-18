@echo OFF
:: Edit this file to select the file formats to use,
:: and then run it to generate the video.

:: Name of input file (any supported video format)
SET INPUT=big_buck_bunny_1080p_h264.mov

:: Name of layout file (bitmap image defining the marker locations)
SET LAYOUT=layout.bmp

:: Name of output file
SET OUTPUT=output.mov

:: Video compression (select one)
:: You can get configuration parameters for each format in the manual or
:: by running e.g. gst-inspect x264enc
:: - video/x-raw-yuv     Uncompressed video (YUV)
:: - video/x-raw-rgb     Uncompressed video (RGB)
:: - x264enc             H.264 video
:: - ffenc_mjpeg         Motion-JPEG video
:: - ffenc_mpeg4         MPEG-4 part 2
:: - ffenc_mpeg2video    MPEG-2 video
:: - ffenc_wmv2          Windows Media Video 8
:: - ffenc_flv           Flash video 
SET COMPRESSION=x264enc speed-preset=4

:: Video container format (select one)
:: The OUTPUT filename should have the corresponding file extension
:: - mp4mux       .MP4
:: - gppmux       .3GP
:: - avimux       .AVI
:: - qtmux        .MOV
:: - asfmux       .ASF
:: - flvmux       .FLV
SET CONTAINER=qtmux

:: Preprocessing (select one or comment all lines to disable)
:: - videoscale ! video/x-raw-yuv,width=XXX,height=XXX                               Resize the video
:: - videorate ! video/x-raw-yuv,framerate=XXX/1                                     Lower FPS by dropping frames
:: - videorate ! videoscale ! video/x-raw-yuv,framerate=XXX/1,width=XXX,height=XXX   Combination of the two
::SET PREPROCESS=! videoscale ! video/x-raw-yuv,width=640,height=480
::SET PREPROCESS=! videorate ! video/x-raw-yuv,framerate=10/1
::SET PREPROCESS=! videorate ! videoscale ! video/x-raw-yuv,framerate=5/1,width=320,height=240

:: Test video generator options
:: Number of frames to process
SET NUM_BUFFERS=64

:: Number of times to repeat the test video
SET REPEAT=5

:: Whether to create a calibration video
:: - off      No calibration sequence
:: - only     Only the calibration sequence, ie. create a separate calibration video
:: - prepend  Put the calibration sequence before the actual video
SET CALIBRATION=prepend

@echo Starting test video generator..

:: Path of the software components
SET PATH=bin\;%PATH%
SET GST_PLUGIN_PATH=lib\gstreamer-0.10;.

:: Store debug info in case something goes wrong
del debug\*.dot debug\*.txt debug\*.png
set GST_DEBUG_DUMP_DOT_DIR=debug
set GST_DEBUG_FILE=debug\log.txt
set GST_DEBUG=*:3

:: Actual command that executes gst-launch
gst-launch -q --gst-plugin-load=GstOFTVG.dll ^
	filesrc location=%INPUT% ! decodebin2 name=decode %PREPROCESS% ! queue ^
	! oftvg location=%LAYOUT% num-buffers=%NUM_BUFFERS% repeat=%REPEAT% calibration=%CALIBRATION% silent=1 ^
	! queue ! ffmpegcolorspace ! %COMPRESSION% ! %CONTAINER% ! filesink location=%OUTPUT%

@echo Done! Press enter to exit.
PAUSE