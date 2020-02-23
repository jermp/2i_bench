import sys, os

collection_filename = sys.argv[1]
output_filename = sys.argv[2]
querylog_basename = sys.argv[3]
results_dirname = sys.argv[4]

codecs = [
"maskedvbyte",
"opt_vbyte",
"bic",
"delta",
"rice",
"pef_opt",
"single_packed_dint",
"optpfor",
"simple16",
"qmx"
,
"slicing"
]

for c in codecs:

    results = c + ".results"
    index_filename = output_filename + "." + c + ".bin"

    os.system("./build_index " + c + " " + collection_filename + " --out " + index_filename + " >> " + results_dirname + "/" + results)

    for i in xrange(0,5):
        os.system("./decode " + c + " " + index_filename + " >> " + results_dirname + "/" + results)

    for suffix in [".2", ".3", ".4", ".5+"]:
        querylog = querylog_basename + suffix
        for i in xrange(0,1):
            os.system("./and " + " " + c + " " + index_filename + " 1000 < " + querylog + " 2>> " + results_dirname + "/" + results)

    for suffix in [".2", ".3", ".4", ".5+"]:
        querylog = querylog_basename + suffix
        for i in xrange(0,1):
            os.system("./or " + " " + c + " " + index_filename + " 1000 < " + querylog + " 2>> " + results_dirname + "/" + results)

    # os.system("rm " + index_filename)
