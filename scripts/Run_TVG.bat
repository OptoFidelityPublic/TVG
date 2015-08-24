@echo OFF

:: OptoFidelity Test Video Generator example script for
:: Windows platforms

:: Edit this file to select the file formats to use,
:: and then run it to generate the video.

:: Name of input file (any supported video format)
SET INPUT=big_buck_bunny_1080p_h264.mp4

:: Name of layout file (bitmap image defining the marker locations)
SET LAYOUT=layout.bmp

:: Name of output file
SET OUTPUT=output.mov

:: Video compression (select one)
:: You can get configuration parameters for each format in the manual or
:: by running e.g. gst-inspect-1.0 x264enc
:: - video/x-raw, format=I420     Uncompressed video (YUV I420)
:: - video/x-raw, format=YUY2     Uncompressed video (YUV YUY2/422)
:: - video/x-raw, format=BGR      Uncompressed video (RGB 24bit)
:: - video/x-raw, format=BGRx     Uncompressed video (RGB 32bit)
:: - x264enc                      H.264 video
:: - avenc_libx265                H.265 video
:: - jpegenc                      Motion-JPEG video
:: - avenc_mpeg4                  MPEG-4 part 2
:: - avenc_mpeg2video             MPEG-2 video
:: - avenc_wmv2                   Windows Media Video 8
:: - avenc_flv                    Flash video 
:: - vp9enc                       Google VP9 video
SET COMPRESSION=x264enc speed-preset=4

:: Video container format (select one)
:: The OUTPUT filename should have the corresponding file extension
:: - mp4mux       .MP4
:: - 3gppmux      .3GP
:: - avimux       .AVI
:: - qtmux        .MOV
:: - asfmux       .ASF
:: - flvmux       .FLV
:: - matroskamux  .MKV
:: - webmmux      .WEBM
SET CONTAINER=qtmux

:: Audio compression
:: - avenc_aac    Advanced Audio Codec
:: - wavenc       Microsoft WAV
:: - vorbisenc    Vorbis audio encoder
:: - avenc_mp2    MPEG audio layer 2
:: - identity     No audio compression (raw PCM)
set AUDIOCOMPRESSION=avenc_aac compliance=-2

:: Preprocessing (select one or comment all lines to disable)
:: - videoscale ! video/x-raw,width=XXX,height=XXX                               Resize the video
:: - videoscale ! video/x-raw,width=XXX,height=XXX,pixel-aspect-ratio=1/1        Resize the video and stretch aspect ratio
:: - videorate ! video/x-raw,framerate=XXX/1                                     Lower FPS by dropping frames
:: - videorate ! videoscale ! video/x-raw,framerate=XXX/1,width=XXX,height=XXX   Combination of the two
::SET PREPROCESS=! videoscale ! video/x-raw,width=640,height=480
::SET PREPROCESS=! videorate ! video/x-raw,framerate=10/1
::SET PREPROCESS=! videorate ! videoscale ! video/x-raw,framerate=5/1,width=320,height=240

:: Number of frames to process (-1 for full length of input video)
SET NUM_BUFFERS=256

:: Interval of lipsync markers in milliseconds (-1 to disable)
SET LIPSYNC=-1

:: If true, only the calibration sequences are applied, ie. this creates
:: a separate calibration video
SET ONLY_CALIBRATION=false

:: If true, white color during calibration sequence is only placed in the marker area
SET RGB6_CALIBRATION=false

:: Parameters (in milliseconds) for:
:: - pre calibration white screen duration 
:: - pre calibration markers duration
:: - post calibration white screen duration
:: If you want no calibration sequence to be added, just set these parameters to 0.
SET PRE_WHITE_DURATION=4000
SET PRE_MARKS_DURATION=1000
SET POST_WHITE_DURATION=5000

:: You can put just the settings you want to change in a file named something.tvg
:: and open it with Run_TVG.bat as the program.
if exist "%1" (
	copy "%1" tmp.bat >NUL
	call tmp.bat
	del tmp.bat
	@echo Loaded parameters from %1
)

@echo Starting test video generator..

call %~dp0gstreamer\env.bat

:: Store debug info in case something goes wrong
set DEBUGDIR=%~dp0debug
del %DEBUGDIR%\*.dot %DEBUGDIR%\*.txt %DEBUGDIR%\*.png 2>NUL
set GST_DEBUG_DUMP_DOT_DIR=%DEBUGDIR%
set GST_DEBUG_FILE=%DEBUGDIR%\log.txt
set GST_DEBUG=*:3
set QUEUE=queue max-size-bytes=100000000 max-size-time=10000000000

:: Actual command that executes gst-launch
gst-launch-1.0 -q ^
	filesrc location="%INPUT%" ! autoaudio_decodebin name=decode %PREPROCESS% ! %QUEUE% ^
        ! oftvg location="%LAYOUT%" num-buffers=%NUM_BUFFERS% only_calibration=%ONLY_CALIBRATION% ^
        rgb6_calibration=%RGB6_CALIBRATION% pre_white_duration=%PRE_WHITE_DURATION% pre_marks_duration=%PRE_MARKS_DURATION% ^
	post_white_duration=%POST_WHITE_DURATION% name=oftvg lipsync=%LIPSYNC% ^
        ! queue ! videoconvert ! %COMPRESSION% ! %QUEUE% ! %CONTAINER% name=mux ! filesink location="%OUTPUT%" ^
        decode. ! audioconvert ! volume volume=0.5 ! %QUEUE% ! oftvg. ^
        oftvg. ! queue ! audioconvert ! %AUDIOCOMPRESSION% ! %QUEUE% ! mux.
        
if not [%2]==[nopause] (
@echo Done! Press enter to exit.
PAUSE
)
