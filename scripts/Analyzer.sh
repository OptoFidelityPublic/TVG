#!/bin/bash

# Analyze a generated test video file.
# Usage: Analyzer.sh <videofile>

SCRIPTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source "$SCRIPTDIR/gstreamer/env.sh"

tvg_analyzer $1
