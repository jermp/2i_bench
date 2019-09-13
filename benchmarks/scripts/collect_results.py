import sys, os

input_filename = sys.argv[1]
output_filename = sys.argv[2]
query_logs_path = sys.argv[3]

codecs = [
"pef_opt",
"bic",
"maskedvbyte",
"slicing",
"delta",
"simple16",
"optpfor",
"single_packed_dint"
"opt_vbyte"
]

for c in codecs:

    results = output_filename + "." + c + ".results"
    index_filename = output_filename + "." + c + ".bin"

    os.system("./build_index " + c + " " + input_filename + " --out " + index_filename + " >> " + results)

    for i in xrange(0,3):
        os.system("./next_geq " + " " + c + " " + index_filename + " " + input_filename + ".docs 2>> " + results)

    for i in xrange(0,5):
        os.system("./and " + " " + c + " " + index_filename + " 1000 < " + query_logs_path + "/" + output_filename + ".queries.mapped4096.2terms.shuffled >> " + results)
    for i in xrange(0,5):
        os.system("./or " + " " + c + " " + index_filename + " 1000 < " + query_logs_path + "/" + output_filename + ".queries.mapped4096.2terms.shuffled >> " + results)
    for i in xrange(0,3):
        os.system("./access " + " " + c + " " + index_filename + " 1000 < " + query_logs_path + "/" + output_filename + ".access.queries.1k >> " + results)
    for i in xrange(0,3):
        os.system("./next_geq " + " " + c + " " + index_filename + " 1000 < " + query_logs_path + "/" + output_filename + ".next_geq.queries.1k >> " + results)
    for i in xrange(0,5):
        os.system("./decode " + c + " " + index_filename + " >> " + results)

    os.system("rm " + index_filename)
