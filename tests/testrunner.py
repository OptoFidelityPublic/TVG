import json
import subprocess
import os.path

class TestRunner:
  def __init__(self, tvg_path):
    self.tvg_path = tvg_path
    self.video_in = os.path.join(self.tvg_path, "big_buck_bunny_1080p_h264.mp4")
    self.layout = os.path.join(self.tvg_path, "layout.bmp")
    
    self.run_tvg = os.path.join(self.tvg_path, "Run_TVG.bat")
    if not os.path.isfile(self.run_tvg):
      self.run_tvg = os.path.join(self.tvg_path, "Run_TVG.sh")
    if not os.path.isfile(self.run_tvg):
      raise Exception("Could not find Run_TVG script in path " + tvg_path)
    
    self.analyzer = os.path.join(self.tvg_path, "Analyzer.bat")
    if not os.path.isfile(self.analyzer):
      self.analyzer = os.path.join(self.tvg_path, "Analyzer.sh")
    if not os.path.isfile(self.analyzer):
      raise Exception("Could not find Analyze script in path " + tvg_path)
  
  def run_test(self, params):
    if 'INPUT' not in params:
      params['INPUT'] = self.video_in
      
    if 'OUTPUT' not in params:
      params['OUTPUT'] = 'output.mov'
      
    if 'LAYOUT' not in params:
      params['LAYOUT'] = self.layout
    
    config = "test_config.tvg"
    f = open(config, 'w')
    for key, value in params.items():
      f.write('SET %s=%s\r\n' % (key, value))
    f.close()
    
    print
    print "===================="
    print "Generating test video"
    print "Running command: " + self.run_tvg + " " + config
    subprocess.check_call([self.run_tvg, config])
    
    print
    print "===================="
    print "Analyzing result file"
    print "Running command: " + self.analyzer + " " + params['OUTPUT'] + " > analyzer_output.txt"
    data = subprocess.check_output([self.analyzer, params['OUTPUT']])
    open('analyzer_output.txt', 'w').write(data)
    
    return json.loads(data)
    
if __name__ == '__main__':
  import sys
  t = TestRunner(sys.argv[1])
  print t.run_test({'OUTPUT': 'output.avi', 'CONTAINER': 'avimux'})
