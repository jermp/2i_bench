`2i_bench` - Inverted Index Benchmark
-------

<!--This is the code used for the experiments in the paper [*Fast Dictionary-based Compression for Inverted Indexes*](http://pages.di.unipi.it/pibiri/papers/WSDM19.pdf) [1], by Giulio Ermanno Pibiri, Matthias Petri and Alistair Moffat.

This guide is meant to provide a brief overview of the library and to illustrate its functionalities through some examples.
##### Table of contents
* [Building the code](#building-the-code)
* [Input data format](#input-data-format)
* [Building the indexes](#building-the-indexes)
* [Vroom environment](#vroom-environment)
* [Benchmark](#benchmark)
* [Authors](#authors)
* [Bibliography](#bibliography)

Building the code
-----------------

The code is tested on Linux Ubuntu with `gcc` 7.3.0. The following dependencies are needed for the build: `CMake` >= 2.8 and `Boost`.

The code is largely based on the [`ds2i`](https://github.com/ot/ds2i) project, so it depends on several submodules. If you have cloned the repository without `--recursive`, you will need to perform the following commands before
building:

    $ git submodule init
    $ git submodule update

To build the code on Unix systems (see file `CMakeLists.txt` for the used compilation flags), it is sufficient to do the following:

    $ mkdir build
    $ cd build
    $ cmake .. -DCMAKE_BUILD_TYPE=Release
    $ make -j[number of jobs]

Setting `[number of jobs]` is recommended, e.g., `make -j4`.

Unless otherwise specified, for the rest of this guide we assume that we type the terminal commands of the following examples from the created directory `build`.


Input data format
-----------------
The collection containing the docID and frequency lists follow the format of [`ds2i`](https://github.com/ot/ds2i), that is all integer lists are prefixed by their length written as 32-bit little-endian unsigned integers:

* `<basename>.docs` starts with a singleton binary sequence where its only
  integer is the number of documents in the collection. It is then followed by
  one binary sequence for each posting list, in order of term-ids. Each posting
  list contains the sequence of docIDs containing the term.

* `<basename>.freqs` is composed of a one binary sequence per posting list, where
  each sequence contains the occurrence counts of the postings, aligned with the
  previous file (note however that this file does not have an additional
  singleton list at its beginning).

The `data` subfolder contains an example of such collection organization, for a total of 113,306 sequences and 3,327,520 postings. The `queries` file is, instead, a collection of 500 (multi-term) queries.

For the following examples, we assume to work with the sample data contained in `data`.

Building the indexes
--------------------

The executables `create_freq_index` should be used to build the indexes, given an input collection. To know the parameters needed by the executable, just type

    $ ./create_freq_index

without any parameters. You will get:

    $ Usage ./create_freq_index:
    $       <index_type> <collection_basename> [output_filename] [--check]

Below we show some examples.

##### Example 1.
The commands

    $ ./create_freq_index single_rect_dint ../test/test_data/test_collection single_rect_dint.bin
    $ ./create_freq_index single_packed_dint ../test/test_data/test_collection single_packed_dint.bin
    $ ./create_freq_index multi_packed_dint ../test/test_data/test_collection multi_packed_dint.bin

can be used to build three DINT indexes that use: a single, rectangular dictionary; a single, packed dictionary and multi, packed dictionaries respectively.

##### Example 2.
The command

    $ ./queries single_packed_dint and single_packed_dint.bin < ../test/test_data/queries

performes the boolean AND queries contained in the data file `queries` over the index serialized to `single_packed_dint.bin`.

Vroom environment
-----------------
The "vroom" environment is designed to test the raw sequential decoding speed
of the encoders. See the folder `vroom_env` and the following example.

##### Example.
After building a `single_packed_dint`, we can encode all the sequences in a collection
(without any blocking mechanism), using the following command

    $ ./encode single_packed_dint ../test/test_data/test_collection.docs --dict dict.test_collection.docs.single_packed.DSF-65536-16 --out test.bin

that serializes all the compressed lists to the file `test.bin`. Then we can decode sequentially all the lists in such file by using

	$ ./decode single_packed_dint test.bin --dict dict.test_collection.docs.single_packed.DSF-65536-16

Benchmark
---------

A comparison between the space of `single_rect`, `single_packed` and `multi_packed` on the provided `test_collection` is shown below (`bpi` stands for "bits per integer").
For this small test collection, we exclude the space for the
dictionaries.
Results have been collected on a machine with an Intel i7-7700 processor clocked at 3.6 GHz and running Linux 4.4.0, 64 bits. The code was compiled using the highest optimization setting (see CMakeLists.txt).

|     **Index**     |**docs [bpi]**  |**freqs [bpi]**  |
|-------------------|---------------:|----------------:|
|`single_rect`      | 5.939          | 3.047           |
|`single_packed`    | 5.939          | 3.047           |
|`multi_packed`     | 4.766          | 2.455           |
|`PEF eps-opt`      | 6.369          | 3.479           |


Authors
-------
* Giulio Ermanno Pibiri, <giulio.pibiri@di.unipi.it>
* Matthias Petri, <matthias.petri@gmail.com>
* Alistair Moffat, <ammoffat@unimelb.edu.au>

Bibliography
------------
* [1] Giulio Ermanno Pibiri, Matthias Petri and Alistair Moffat, *Fast Dictionary-based Compression for Inverted Indexes*. In the Proceedings of the 12-th ACM Conference on Web Search and Data Mining (WSDM 2019).-->