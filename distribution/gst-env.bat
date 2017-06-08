set GSTREAMER_DIR=%~dp0
set PATH=%GSTREAMER_DIR%bin;%PATH%
set GST_PLUGIN_PATH=
set GST_PLUGIN_SYSTEM_PATH=%GSTREAMER_DIR%lib\gstreamer-1.0
set GST_PLUGIN_SCANNER=%GSTREAMER_DIR%\bin\gst-plugin-scanner
set GST_REGISTRY=%GSTREAMER_DIR%lib\registry.bin


