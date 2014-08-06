import testcases
import testrunner
import sys


if len(sys.argv) != 2:
  print "Usage: " + sys.argv[0] + " .../path/to/tvg/directory"
  sys.exit(1)

tr = testrunner.TestRunner(sys.argv[1])

errors = False
for testcase in testcases.TestCase.__subclasses__():
  t = testcase()
  t.run(tr)
  
  if t.errors:
    errors = True

print "==============="
if errors:
  print "Some tests failed!"
else:
  print "All tests ok"
