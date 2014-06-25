export GSTREAMER_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
export PATH=$GSTREAMER_DIR/bin:$PATH
export GST_PLUGIN_PATH=
export GST_PLUGIN_SYSTEM_PATH=$GSTREAMER_DIR/lib/gstreamer-1.0
export GST_REGISTRY=$GSTREAMER_DIR/lib/registry.bin
export LD_LIBRARY_PATH=$GSTREAMER_DIR/lib:$LD_LIBRARY_PATH

