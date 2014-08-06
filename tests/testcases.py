import sys
import inspect

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

class TestBasicVideo(TestCase):
  def run(self, tr):
    params = {
      'COMPRESSION':       'jpegenc',
      'CONTAINER':         'avimux',
      'AUDIOCOMPRESSION':  'avenc_ac3',
      'NUM_BUFFERS':       '256',
      'LIPSYNC':           '-1',
      'CALIBRATION':       'off',
      'OUTPUT':            'output.avi'
    }
    
    r = tr.run_test(params)
    
    self.assert_equals(r['demuxer'],         'avidemux')
    self.assert_equals(r['video_codec'],     'jpegdec')
    self.assert_equals(r['audio_codec'],     'a52dec')
    self.assert_equals(r['resolution'],      [1920, 1080])
    self.assert_equals(r['framerate'],       24.0)
    self.assert_equals(r['markers_found'],   27)
    self.assert_equals(r['video_structure']['content_frames'], 256)
    self.assert_equals(r['lipsync']['audio_markers'], 0)
    self.assert_equals(r['lipsync']['video_markers'], 0)
    self.assert_equals(r['warnings'], [])

