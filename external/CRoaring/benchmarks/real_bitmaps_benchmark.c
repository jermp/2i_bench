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

void die(const char *func, const char *filename) {
    fprintf(stderr, "%s(%s): %s\n", func, filename, strerror(errno));
    exit(1);
}

/**
 * Once you have collected all the integers, build the bitmaps.
 */
static roaring_bitmap_t **create_all_bitmaps(size_t *howmany,
                                             uint32_t **numbers, size_t count,
                                             bool runoptimize,
                                             bool copy_on_write) {
    size_t bytes = 0;
    size_t total_integers = 0;

    if (numbers == NULL) return NULL;
    printf("Constructing %d  bitmaps.\n", (int)count);
    roaring_bitmap_t **answer = malloc(sizeof(roaring_bitmap_t *) * count);
    for (size_t i = 0; i < count; i++) {
        fflush(stdout);
        total_integers += howmany[i];
        answer[i] = roaring_bitmap_of_ptr(howmany[i], numbers[i]);
        if (runoptimize) {
            roaring_bitmap_run_optimize(answer[i]);
        }
        roaring_bitmap_shrink_to_fit(answer[i]);
        roaring_bitmap_set_copy_on_write(answer[i], copy_on_write);
        bytes += roaring_bitmap_size_in_bytes(answer[i]);
    }

    printf("consuming a total of %zu bytes for %zu integers: %f [bpi]\n", bytes,
           total_integers, bytes * 8.0 / total_integers);
    return answer;
}

static void printusage(char *command) {
    printf(
        " Try %s directory \n where directory could be "
        "benchmarks/realdata/census1881\n",
        command);
    ;
}

#define KNRM "\x1B[0m"
#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"
#define KYEL "\x1B[33m"
#define KBLU "\x1B[34m"
#define KMAG "\x1B[35m"
#define KCYN "\x1B[36m"
#define KWHT "\x1B[37m"

/* Original code by D. Lemire */
int main(int argc, char **argv) {
    int c;
    const char *extension = ".txt";
    bool copy_on_write = false;
    bool runoptimize = false;
    while ((c = getopt(argc, argv, "e:h")) != -1) switch (c) {
            case 'e':
                extension = optarg;
                break;
            case 'h':
                printusage(argv[0]);
                return 0;
            default:
                abort();
        }
    if (optind >= argc) {
        printusage(argv[0]);
        return -1;
    }
    char *dirname = argv[optind];
    size_t count;

    size_t *howmany = NULL;
    uint32_t **numbers =
        read_all_integer_files(dirname, extension, &howmany, &count);
    if (numbers == NULL) {
        printf(
            "I could not find or load any data file with extension %s in "
            "directory %s.\n",
            extension, dirname);
        return -1;
    }

    uint64_t cycles_start = 0, cycles_final = 0;

    RDTSC_START(cycles_start);
    roaring_bitmap_t **bitmaps =
        create_all_bitmaps(howmany, numbers, count, runoptimize, copy_on_write);
    RDTSC_FINAL(cycles_final);
    if (bitmaps == NULL) return -1;
    printf("Loaded %d bitmaps from directory %s \n", (int)count, dirname);

    printf("Creating %zu bitmaps took %" PRIu64 " cycles\n", count,
           cycles_final - cycles_start);

    // ANDing consecutive pairs
    double total_usecs = 0.0;
    // first run if for warming up
    static const int runs = 10 + 1;
    roaring_bitmap_t *tempand = NULL;
    for (int run = 0; run != runs; ++run) {
        double start = get_time_usecs();
        for (int i = 0; i < (int)count - 1; ++i) {
            tempand = roaring_bitmap_and(bitmaps[i], bitmaps[i + 1]);
            do_not_optimize_away(tempand);
        }
        double end = get_time_usecs();
        double elapsed = end - start;
        if (run) {
            total_usecs += elapsed;
        }
    }
    roaring_bitmap_free(tempand);
    printf(
        " %d successive bitmaps intersections took %lf [musecs] (avg. among %d "
        "runs)\n",
        count - 1, total_usecs / (runs - 1), runs - 1);

    // uint64_t successive_and = 0;
    // // ANDing consecutive pairs
    // static const int runs = 10;
    // roaring_bitmap_t *tempand = NULL;
    // for (int run = 0; run != runs; ++run) {
    //     RDTSC_START(cycles_start);
    //     for (int i = 0; i < (int)count - 1; ++i) {
    //         tempand = roaring_bitmap_and(bitmaps[i], bitmaps[i + 1]);
    //     }
    //     RDTSC_FINAL(cycles_final);
    //     successive_and += cycles_final - cycles_start;
    // }
    // roaring_bitmap_free(tempand);
    // printf(" %zu successive bitmaps intersections took %" PRIu64
    //        " cycles (avg. among %d runs)\n",
    //        count - 1, successive_and / runs, runs);

    for (int i = 0; i < (int)count; ++i) {
        free(numbers[i]);
        numbers[i] = NULL;  // paranoid
        roaring_bitmap_free(bitmaps[i]);
        bitmaps[i] = NULL;  // paranoid
    }
    free(bitmaps);
    free(howmany);
    free(numbers);

    return 0;
}
