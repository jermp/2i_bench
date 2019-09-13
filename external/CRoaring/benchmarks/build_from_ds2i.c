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

uint32_t min_size = 4096;

void save(int fd, const char *buf, size_t len) {
    while (len > 0) {
        ssize_t res = write(fd, buf, len);
        if (res == -1) {
            printf("write failed\n");
            exit(1);
        }
        buf += res;
        len -= res;
    }
}

size_t insert_padding(int fd, size_t bytes) {
    size_t mod = bytes % 32;
    size_t pad = 0;
    if (mod) {
        pad = 32 - mod;
        char *buf = malloc(pad);
        // for (size_t i = 0; i != pad; ++i) {
        //     buf[i] = 0;
        // }

        for (size_t p = pad; p != 0;) {
            ssize_t res = write(fd, buf, p);
            if (res == -1) {
                printf("write failed\n");
                exit(1);
            }
            buf += res;
            p -= res;
        }

        free(buf - pad);
    }
    return pad;
}

void build_from_ds2i(char const *input_filename, char const *output_filename,
                     bool runoptimize) {
    int in_fd = open(input_filename, O_RDONLY);
    if (in_fd == -1) {
        printf("cannot open %s\n", input_filename);
        exit(1);
    }

    struct stat st;
    if (fstat(in_fd, &st) == -1) {
        printf("cannot stat %s\n", input_filename);
        exit(1);
    }

    printf("total bytes in file: %lld\n", st.st_size);

    uint32_t *ptr =
        (uint32_t *)mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, in_fd, 0);
    if (!ptr) {
        printf("mmap failed\n");
        exit(1);
    }

    roaring_statistics_t global_stats;
    memset(&global_stats, 0, sizeof(roaring_statistics_t));
    size_t bytes = 0;
    size_t total_integers = 0;

    printf("Constructing bitmaps from file '%s'...\n", input_filename);
    uint32_t const *end = ptr + st.st_size / sizeof(uint32_t);

    // skip 4 bytes that are always 1
    // skip 4 bytes that contain the universe size
    ++ptr;
    uint32_t universe = *ptr;
    printf("universe size: %d\n", universe);
    ++ptr;

    uint32_t num_bitmaps = 0;
    {
        // compute number of lists to encode
        uint32_t *begin = ptr;
        while (begin < end) {
            uint32_t n = *begin;
            if (n > min_size) {
                ++num_bitmaps;
            }
            begin += n + 1;
        }
    }

    printf("encoding %d bitmaps...\n", num_bitmaps);

    // assume each Roaring bitmap takes less than 4GB
    uint32_t *sizes = (uint32_t *)malloc(num_bitmaps * sizeof(uint32_t));
    roaring_bitmap_t **bitmaps =
        (roaring_bitmap_t **)malloc(num_bitmaps * sizeof(roaring_bitmap_t *));

    num_bitmaps = 0;
    while (ptr < end) {
        uint32_t n = *ptr;
        ++ptr;

        if (n > min_size) {
            bitmaps[num_bitmaps] = roaring_bitmap_of_ptr(n, ptr);
            total_integers += n;

            if (runoptimize) {
                roaring_bitmap_run_optimize(bitmaps[num_bitmaps]);
            }

            // roaring_bitmap_shrink_to_fit(bitmaps[num_bitmaps]);
            size_t size =
                roaring_bitmap_frozen_size_in_bytes(bitmaps[num_bitmaps]);
            bytes += size;
            sizes[num_bitmaps] = size;

            // collect statistics for each bitmap
            roaring_statistics_t tmp_stats;
            roaring_bitmap_statistics(bitmaps[num_bitmaps], &tmp_stats);

            // accumulate statistics
            global_stats.n_containers += tmp_stats.n_containers;
            global_stats.n_array_containers += tmp_stats.n_array_containers;
            // global_stats.n_run_containers += tmp_stats.n_run_containers;
            global_stats.n_bitset_containers += tmp_stats.n_bitset_containers;

            global_stats.n_values_array_containers +=
                tmp_stats.n_values_array_containers;
            // global_stats.n_values_run_containers +=
            //     tmp_stats.n_values_run_containers;
            global_stats.n_values_bitset_containers +=
                tmp_stats.n_values_bitset_containers;

            global_stats.n_bytes_array_containers +=
                tmp_stats.n_bytes_array_containers;
            // global_stats.n_bytes_run_containers +=
            //     tmp_stats.n_bytes_run_containers;
            global_stats.n_bytes_bitset_containers +=
                tmp_stats.n_bytes_bitset_containers;

            ++num_bitmaps;
            if (num_bitmaps % 1000 == 0) {
                printf("processed %d bitmaps\n", num_bitmaps);
            }
        }

        ptr += n;
    }

    if (munmap((char *)ptr - st.st_size, st.st_size) == -1) {
        printf("unmap failed\n");
        exit(1);
    }

    close(in_fd);
    printf("processed %d bitmaps\n", num_bitmaps);
    printf("processed %zu integers\n", total_integers);

    printf("consuming a total of %zu bytes for %zu integers: %f [bpi]\n", bytes,
           total_integers, bytes * 8.0 / total_integers);

    printf("array containers: %f%%\n",
           global_stats.n_array_containers * 100.0 / global_stats.n_containers);
    // printf("run containers: %f%%\n",
    //        global_stats.n_run_containers * 100.0 /
    //        global_stats.n_containers);
    printf("bitset containers: %f%%\n", global_stats.n_bitset_containers *
                                            100.0 / global_stats.n_containers);

    printf("integers in array containers: %f%%\n",
           global_stats.n_values_array_containers * 100.0 / total_integers);
    // printf("integers in run containers: %f%%\n",
    //        global_stats.n_values_run_containers * 100.0 / total_integers);
    printf("integers in bitset containers: %f%%\n",
           global_stats.n_values_bitset_containers * 100.0 / total_integers);

    printf("bytes for array containers: %f%%\n",
           global_stats.n_bytes_array_containers * 100.0 / bytes);
    // printf("bytes for run containers: %f%%\n",
    //        global_stats.n_bytes_run_containers * 100.0 / bytes);
    printf("bytes for bitset containers: %f%%\n",
           global_stats.n_bytes_bitset_containers * 100.0 / bytes);

    printf("serializing bitmaps to '%s'...\n", output_filename);

    int out_fd = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (out_fd == -1) {
        printf("cannot open %s\n", output_filename);
        exit(1);
    }

    // format is:
    // num_num_bitmaps (4 bytes)
    // universe (4 bytes)
    // sizes (8 * num_num_bitmaps bytes)
    // bitmaps

    if (write(out_fd, (char const *)&num_bitmaps, sizeof(num_bitmaps)) == -1) {
        printf("failed write\n");
        exit(1);
    }
    if (write(out_fd, (char const *)&universe, sizeof(universe)) == -1) {
        printf("failed write\n");
        exit(1);
    }
    bytes = sizeof(num_bitmaps) + sizeof(universe);

    for (uint32_t i = 0; i != num_bitmaps; ++i) {
        uint32_t size = sizes[i];
        if (write(out_fd, (char const *)&size, sizeof(uint32_t)) == -1) {
            printf("write failed\n");
            exit(1);
        }
        bytes += sizeof(uint32_t);
    }

    size_t padding_bytes = 0;
    for (uint32_t i = 0; i != num_bitmaps; ++i) {
        uint32_t size = sizes[i];

        // pranoid
        if (size != roaring_bitmap_frozen_size_in_bytes(bitmaps[i])) {
            printf(
                "sizes[%d] != "
                "roaring_bitmap_frozen_size_in_bytes(bitmaps[%d]))\n",
                i, i);
            exit(1);
        }

        // roaring_bitmap_frozen_view() must begin aligned by 32 bytes
        size_t pad = insert_padding(out_fd, bytes);
        padding_bytes += pad;
        bytes += pad;
        bytes += size;
        char *buf = malloc(size);
        if (!buf) {
            printf("malloc failed\n");
            exit(1);
        }
        roaring_bitmap_frozen_serialize(bitmaps[i], buf);
        save(out_fd, buf, size);
        free(buf);
        roaring_bitmap_free(bitmaps[i]);
    }

    free(sizes);
    free(bitmaps);
    close(out_fd);

    printf(
        "written a total of %zu bytes, out of which %zu for padding (%f%%)\n",
        bytes, padding_bytes, padding_bytes * 100.0 / bytes);
}

int main(int argc, char **argv) {
    int mandatory = 3;
    if (argc < mandatory) {
        printf(
            "Usage: %s input_filename output_filename"
            " --runopt\n",
            argv[0]);
        return 1;
    }

    const char *input_filename = argv[1];
    const char *output_filename = argv[2];

    bool runoptimize = false;
    if (argc > mandatory && strcmp(argv[mandatory], "--runopt") == 0) {
        runoptimize = true;
    }

    build_from_ds2i(input_filename, output_filename, runoptimize);

    return 0;
}