import sys
import inspect
import os.path

class TestCase(object):
  '''Base class for test cases'''
  def __init__(self):
    self.errors = False
  
  def run(tr):
    '''tr is TestRunner instance.'''
    raise NotImplemented()
  
  def assert_equals(self, a, b):
    if a != b:
      caller = inspect.stack()[-2]
      print "%s:%s in %s" % caller[1:4]
      print "   %s" % caller[-2][0].strip()
      print "   value is " + repr(a) + ", expected " + repr(b)
      self.errors = True
  
  def assert_range(self, a, minval, maxval):
    if a < minval or a > maxval:
      caller = inspect.stack()[-2]
      print "%s:%s in %s" % caller[1:4]
      print "   %s" % caller[-2][0].strip()
      print "   value is " + repr(a) + ", expected to be in range " + repr(minval) + " to " + repr(maxval)
      self.errors = True

class TestBasicVideo(TestCase):
  def run(self, tr):
    params = {
      'COMPRESSION':       'jpegenc',
      'CONTAINER':         'avimux',
      'AUDIOCOMPRESSION':  'avenc_ac3',
      'NUM_BUFFERS':       '256',
      'LIPSYNC':           '-1',
      'CALIBRATION':       'prepend',
      'OUTPUT':            'output.avi'
    }
    
    r = tr.run_test(params)
    
    self.assert_equals(r['demuxer'],         'avidemux')
    self.assert_equals(r['video_codec'],     'jpegdec')
    self.assert_equals(r['audio_codec'],     'a52dec')
    self.assert_equals(r['resolution'],      [1920, 1080])
    self.assert_equals(r['framerate'],       24.0)
    self.assert_equals(r['markers_found'],   27)
    self.assert_equals(r['markers'][3]['interval'], 1)
    self.assert_equals(r['markers'][4]['interval'], 2)
    self.assert_equals(r['markers'][5]['interval'], 4)
    self.assert_equals(r['markers'][6]['interval'], 8)
    self.assert_equals(r['markers'][7]['interval'], 16)
    self.assert_equals(r['markers'][8]['interval'], 32)
    self.assert_equals(r['markers'][9]['interval'], 64)
    self.assert_equals(r['markers'][10]['interval'], 128)
    self.assert_range(r['video_structure']['header_frames'], 80, 300)
    self.assert_equals(r['video_structure']['content_frames'], 256)
    self.assert_equals(r['video_structure']['trailer_frames'], 0)
    self.assert_equals(r['lipsync']['audio_markers'], 0)
    self.assert_equals(r['lipsync']['video_markers'], 0)
    self.assert_equals(r['warnings'], [])

class TestLipsync(TestCase):
  def run(self, tr):
    params = {
      'COMPRESSION':       'x264enc speed-preset=2',
      'CONTAINER':         'qtmux',
      'AUDIOCOMPRESSION':  'identity',
      'NUM_BUFFERS':       '240',
      'LIPSYNC':           '2000',
      'CALIBRATION':       'both',
      'OUTPUT':            'output.mov',
      'PREPROCESS':        '! videoscale ! video/x-raw,width=640,height=480',
      'LAYOUT':            os.path.join(tr.tvg_path, "layout_fpsonly.bmp")
    }
    
    r = tr.run_test(params)
    
    self.assert_equals(r['demuxer'],         'qtdemux')
    self.assert_equals(r['video_codec'],     'avdec_h264')
    self.assert_equals(r['audio_codec'],     '(null)')
    self.assert_equals(r['resolution'],      [640, 480])
    self.assert_equals(r['framerate'],       24.0)
    self.assert_equals(r['markers_found'],   1)
    self.assert_range(r['video_structure']['header_frames'], 80, 300)
    self.assert_equals(r['video_structure']['content_frames'], 240)
    self.assert_range(r['video_structure']['trailer_frames'], 80, 300)
    self.assert_equals(r['lipsync']['audio_markers'], 5)
    self.assert_equals(r['lipsync']['video_markers'], 5)
    self.assert_range(r['lipsync']['audio_delay_min_ms'], -1.0, 1.0)
    self.assert_range(r['lipsync']['audio_delay_max_ms'], -1.0, 1.0)
    self.assert_equals(r['warnings'], [])
