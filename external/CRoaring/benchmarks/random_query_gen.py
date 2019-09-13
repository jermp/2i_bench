import random
import sys

# generate n pairs of integers, where each integer is random between [0,u)

n = int(sys.argv[1])
u = int(sys.argv[2])

if u < 1:
    print "u must be greater than 0"
    exit()

for i in range(0, n):
    x = 0
    y = 0
    while x == y:
        x = random.randint(0, u - 1)
        y = random.randint(0, u - 1)
    print str(x) + "\t" + str(y)