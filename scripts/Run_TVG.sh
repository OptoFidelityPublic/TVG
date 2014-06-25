#!/bin/bash

# OptoFidelity Test Video Generator example script for
# Linux platforms.

# Edit this file to select the file formats to use,
# and then run it to generate the video.

# Name of input file (any supported video format)
INPUT="big_buck_bunny_1080p_h264.mp4"

# Name of layout file (bitmap image defining the marker locations)
LAYOUT="layout.bmp"

# Name of output file
OUTPUT="output.mov"

# Video compression (select one)
# You can get configuration parameters for each format in the manual or
# by running e.g. gst-inspect x264enc
# - video/x-raw-yuv     Uncompressed video (YUV)
# - video/x-raw-rgb     Uncompressed video (RGB)
# - x264enc             H.264 video
# - x265enc             H.265 video
# - ffenc_mjpeg         Motion-JPEG video
# - ffenc_mpeg4         MPEG-4 part 2
# - ffenc_mpeg2video    MPEG-2 video
# - ffenc_wmv2          Windows Media Video 8
# - ffenc_flv           Flash video 
# - vp9enc              Google VP9 video
COMPRESSION="x264enc speed-preset=4"

# Video container format (select one)
# The OUTPUT filename should have the corresponding file extension
# - mp4mux       .MP4
# - gppmux       .3GP
# - avimux       .AVI
# - qtmux        .MOV
# - asfmux       .ASF
# - flvmux       .FLV
CONTAINER="qtmux"

# Audio compression
# - avenc_aac    Advanced Audio Codec
# - wavenc       Microsoft WAV
# - vorbisenc    Vorbis audio encoder
# - avenc_mp2    MPEG audio layer 2
AUDIOCOMPRESSION="avenc_aac compliance=-2"

# Video preprocessing (select one or comment all lines to disable)
# - videoscale ! video/x-raw-yuv,width=XXX,height=XXX                               Resize the video
# - videoscale ! video/x-raw-yuv,width=XXX,height=XXX,pixel-aspect-ratio=1/1        Resize the video and stretch aspect ratio
# - videorate ! video/x-raw-yuv,framerate=XXX/1                                     Lower FPS by dropping frames
# - videorate ! videoscale ! video/x-raw-yuv,framerate=XXX/1,width=XXX,height=XXX   Combination of the two
# set PREPROCESS="! videoscale ! video/x-raw-yuv,width=640,height=480"
# set PREPROCESS="! videorate ! video/x-raw-yuv,framerate=10/1"
# set PREPROCESS="! videorate ! videoscale ! video/x-raw-yuv,framerate=5/1,width=320,height=240"

# Number of frames to process (-1 for full length of input video)
NUM_BUFFERS=-1

# Interval of lipsync markers in milliseconds (-1 to disable)
LIPSYNC=-1

# Whether to create a calibration video
# - off      No calibration sequence
# - only     Only the calibration sequence, ie. create a separate calibration video
# - prepend  Put the calibration sequence before the actual video
# - both     Both at start and end (for Video Multimeter)
CALIBRATION="off"

# You can put just the settings you want to change in a file named something.tvg
# and open it with Run_TVG.sh as the program.
if [ -e "$1" ]
then eval $(cat $1 | sed 's/^::/#/' | sed 's/SET ([^=]+)=(.+)/\\1="\\2"/')
     echo Loaded parameters from $1
fi

echo Starting test video generator..

SCRIPTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "$SCRIPTDIR/gstreamer/env.sh"

# Store debug info in case something goes wrong
DEBUGDIR="$SCRIPTDIR/debug"
rm -f $DEBUGDIR/*.dot $DEBUGDIR/*.txt $DEBUGDIR/*.png
export GST_DEBUG_DUMP_DOT_DIR=$DEBUGDIR
export GST_DEBUG_FILE=$DEBUGDIR/log.txt
export GST_DEBUG=*:4

# Actual command that executes gst-launch
gst-launch-1.0 -q \
        filesrc location="$INPUT" ! decodebin name=decode $PREPROCESS ! queue \
        ! oftvg location="$LAYOUT" num-buffers=$NUM_BUFFERS calibration=$CALIBRATION \
                name=oftvg lipsync=$LIPSYNC \
        ! queue ! videoconvert ! $COMPRESSION ! queue ! $CONTAINER name=mux ! filesink location="$OUTPUT" \
        decode. ! audioconvert ! audioresample ! queue ! adder name=audiomix ! $AUDIOCOMPRESSION ! queue ! mux. \
        oftvg. ! audioconvert ! audio/x-raw,channels=2 ! queue ! audiomix.

echo Done!
