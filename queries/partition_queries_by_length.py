import sys, os, random

input_filename = sys.argv[1]
output_directory = sys.argv[2]

if not os.path.exists(output_directory):
    os.makedirs(output_directory)

files = [open(output_directory + "/queries." + str(i), "w") for i in range(2, 5)]
all_others = open(output_directory + "/queries.5+", "w")

strings = [[] for i in range(3)]
all_others_strings = []

lines = 0
with open(input_filename, 'r') as f:
    for line in f:
        x = line.rstrip('\n').split()
        l = len(x)
        string = ' '.join(x[0:l]) + '\n'
        if l >= 5:
            all_others_strings.append(string)
        else:
            strings[l - 2].append(string)
        lines += 1
        if lines % 1000 == 0:
            print("processed " + str(lines) + " lines")

for i in range(3):
    random.shuffle(strings[i])
    for s in strings[i]:
        files[i].write(s)
    files[i].close()

random.shuffle(all_others_strings)
for s in all_others_strings:
    all_others.write(s)
all_others.close()
