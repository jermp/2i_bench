import sys, os

input_filename = sys.argv[1]
output_filename = sys.argv[2]
querylog = sys.argv[3]

codecs = [
"maskedvbyte",
"opt_vbyte",
"bic",
"delta",
"opt_delta",
"rice",
"pef_opt",
"single_packed_dint",
"optpfor",
"simple16",
"qmx",
"slicing"
]

for c in codecs:

    results = output_filename + "." + c + ".results"
    index_filename = output_filename + "." + c + ".bin"

    os.system("./build_index " + c + " " + input_filename + " --out " + index_filename + " >> " + results)

    # for i in xrange(0,5):
    #     os.system("./and " + " " + c + " " + index_filename + " 1000 < " + querylog + " >> " + results)

    # for i in xrange(0,5):
    #     os.system("./decode " + c + " " + index_filename + " >> " + results)

    for i in xrange(0,3):
        os.system("./or " + " " + c + " " + index_filename + " 1000 < " + querylog + " >> " + results)

    os.system("rm " + index_filename)
