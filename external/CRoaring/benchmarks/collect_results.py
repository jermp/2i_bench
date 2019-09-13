import sys, os

input_filename = sys.argv[1]
output_filename = sys.argv[2]
query_logs_path = sys.argv[3]

c = "roaring_runopt"
results = output_filename + "." + c + ".results"
index_filename = output_filename + "." + c + ".bin"

os.system("./benchmarks/build_from_ds2i " + input_filename + " " + index_filename + " --runopt >> " + results)

for i in xrange(0,5):
    os.system("./benchmarks/pair_wise_intersect " + index_filename + " 1000 < " + query_logs_path + "/" + output_filename + ".queries.mapped4096.2terms.shuffled >> " + results)
for i in xrange(0,5):
    os.system("./benchmarks/pair_wise_union " + index_filename + " 1000 < " + query_logs_path + "/" + output_filename + ".queries.mapped4096.2terms.shuffled >> " + results)
for i in xrange(0,3):
    os.system("./benchmarks/select " + index_filename + " 1000 < " + query_logs_path + "/" + output_filename + ".access.queries.1k >> " + results)
for i in xrange(0,3):
    os.system("./benchmarks/next_geq " + index_filename + " 1000 < " + query_logs_path + "/" + output_filename + ".next_geq.queries.1k >> " + results)
for i in xrange(0,5):
    os.system("./benchmarks/decoding " + index_filename + " >> " + results)

os.system("rm " + index_filename)
