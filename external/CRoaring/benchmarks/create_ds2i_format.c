#define _GNU_SOURCE
#include <roaring/roaring.h>
#include "benchmark.h"
#include "numbersfromtextfiles.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

void write_to_file(FILE *out, uint32_t x) {
    fwrite((char const *)&x, sizeof(uint32_t), 1, out);
}

void create_ds2i_format(size_t lists, uint32_t universe,
                        size_t const *list_sizes, uint32_t **integers,
                        char const *output_filename) {
    char *docs_filename = malloc(strlen(output_filename) + 5 + 1);
    strcpy(docs_filename, output_filename);
    FILE *docs = fopen(strcat(docs_filename, ".docs"), "wb");
    if (!docs) {
        printf("Error: cannot open file");
        exit(1);
    }

    char *freqs_filename = malloc(strlen(output_filename) + 6 + 1);
    strcpy(freqs_filename, output_filename);
    FILE *freqs = fopen(strcat(freqs_filename, ".freqs"), "wb");
    if (!freqs) {
        printf("Error: cannot open file");
        exit(1);
    }

    uint32_t one = 1;
    write_to_file(docs, one);
    write_to_file(docs, universe);

    for (size_t i = 0; i != lists; ++i) {
        uint32_t n = list_sizes[i];
        write_to_file(docs, n);
        write_to_file(freqs, n);
        for (size_t j = 0; j != n; ++j) {
            write_to_file(docs, integers[i][j]);
            write_to_file(freqs, one);
        }
    }

    fclose(docs);
    fclose(freqs);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: %s dirname output_filename ", argv[0]);
        return 1;
    }

    const char *extension = ".txt";
    char const *dirname = argv[1];  // e.g. benchmark/realdata/census-income
    char const *output_filename = argv[2];

    size_t count = 0;
    size_t *howmany = NULL;
    uint32_t **numbers =
        read_all_integer_files(dirname, extension, &howmany, &count);

    uint32_t universe = 0;
    for (size_t i = 0; i != count; ++i) {
        for (size_t j = 0; j != howmany[i]; ++j) {
            if (numbers[i][j] > universe) {
                universe = numbers[i][j];
            }
        }
    }

    if (numbers == NULL) {
        printf(
            "I could not find or load any data file with extension %s in "
            "directory %s.\n",
            extension, dirname);
        return -1;
    }

    create_ds2i_format(count, universe, howmany, numbers, output_filename);

    return 0;
}