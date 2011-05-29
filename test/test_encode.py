import sys
from webp.encode import encode

print "default"
encode(sys.argv[1], sys.argv[2])
print "set quality:50"
encode(sys.argv[1], sys.argv[2], quality=50)
print "set method:0(fast)"
encode(sys.argv[1], sys.argv[2], method=0)
