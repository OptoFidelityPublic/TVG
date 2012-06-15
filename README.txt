This is the source code distribution of OptoFidelity Test Video Generator.
Use it only if you want to compile TVG yourself - there are binary distributions
available at http://code.google.com/p/oftvg/downloads/list.

--

Compiling:

1. Download the latest GStreamer GPL and GStreamer SDK .zips from
http://code.google.com/p/ossbuild-vs2010/downloads/list

2. Extract those .zips under the TVG directory, so you should get directories
called 'gstreamer-gpl' and 'gstreamer-sdk' there, with files directly under them.

3. Open GstOFTVG/GstOFTVG.sln in Visual Studio 2010 and build it in Release mode.

4. Run distribution/make_distribution.bat to copy all the files into a deployable
directory structure. The folder will be named like distribution/tvg-<git id>

--

Publishing:

1. Remember to update version number in documentation.
2. Create a new tag:  git tag -a 0.4
3. Verify that everything is pushed to repository: git push origin master --tags
4. Run make_distribution.bat, which should now create a folder called tvg-0.4.
5. Zip up the folder and test it.


