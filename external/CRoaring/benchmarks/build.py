import sys, os

input_filename = sys.argv[1]
output_filename = sys.argv[2]
runopt = int(sys.argv[3])
densities = ["0.01", "0.001", "0.0001"]

if runopt == 0:
    results = output_filename + ".building_log"
    for d in densities:
        os.system("./benchmarks/build_from_ds2i " + input_filename + " " + output_filename + "." + d + ".bin" + " " + d  + " >> " + results)
else:
    results = output_filename + ".runopt.building_log"
    for d in densities:
        os.system("./benchmarks/build_from_ds2i " + input_filename + " " + output_filename + "." + d + ".runopt.bin" + " " + d  + " --runopt >> " + results)