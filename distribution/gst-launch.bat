:: This is a wrapper script to maintain compatibility with old Run_TVG.bat variants.
call %~dp0..\gstreamer\env.bat
gst-launch %*
