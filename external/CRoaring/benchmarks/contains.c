#define _GNU_SOURCE
#include <roaring/roaring.h>
#include "benchmark.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: %s binary_filename num_queries < query_filename\n",
               argv[0]);
        return 1;
    }

    char const *binary_filename = argv[1];
    uint32_t num_queries = atoi(argv[2]);

    int fd = open(binary_filename, O_RDONLY);
    if (fd == -1) {
        printf("cannot open %s\n", binary_filename);
        exit(1);
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        printf("cannot stat %s\n", binary_filename);
        exit(1);
    }

    uint32_t num_bitmaps = 0;
    uint32_t universe = 0;
    read(fd, (char *)&num_bitmaps, sizeof(num_bitmaps));
    read(fd, (char *)&universe, sizeof(universe));
    printf("num_bitmaps %d\n", num_bitmaps);
    printf("universe %d\n", universe);

    uint32_t *sizes = (uint32_t *)malloc(num_bitmaps * sizeof(uint32_t));
    roaring_bitmap_t const **bitmaps = (roaring_bitmap_t const **)malloc(
        num_bitmaps * sizeof(roaring_bitmap_t const *));

    char *ptr = (char *)mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (!ptr) {
        printf("mmap failed\n");
        exit(1);
    }

    ptr += sizeof(num_bitmaps) + sizeof(universe);
    for (uint32_t i = 0; i != num_bitmaps; ++i) {
        sizes[i] = *((uint32_t *)ptr);
        ptr += sizeof(uint32_t);
    }

    printf("allocating bitmaps...\n");
    for (uint32_t i = 0; i != num_bitmaps; ++i) {
        uint32_t size = sizes[i];
        ptr = read_padding(ptr);
        bitmaps[i] = roaring_bitmap_frozen_view(ptr, size);
        if (bitmaps[i] == NULL) {
            printf("ERROR in roaring_bitmap_frozen_view \n");
            exit(1);
        }
        ptr += size;
    }

    free(sizes);

    if (num_bitmaps == 1) {
        printf("need at least two bitmaps to perform intersection\n");
        return 1;
    }

    query *queries = (query *)malloc(num_queries * sizeof(query));
    printf("reading queries...\n");
    for (uint32_t i = 0; i != num_queries; ++i) {
        int x = scanf("%d", &queries[i].i);
        int y = scanf("%d", &queries[i].j);
        if (x == EOF || y == EOF) {
            num_queries = i;
            break;
        }
    }

    printf("performing %d contains queries...\n", num_queries);

    double total_usecs = 0.0;
    size_t total = 0;
    // first run if for warming up
    static const int runs = 3 + 1;
    for (int run = 0; run != runs; ++run) {
        double start = get_time_usecs();
        for (uint32_t i = 0; i != num_queries; ++i) {
            total +=
                roaring_bitmap_contains(bitmaps[queries[i].i], queries[i].j);
        }
        double end = get_time_usecs();
        double elapsed = end - start;
        if (run) {
            total_usecs += elapsed;
        }
    }

    printf("%zu\n", total);

    printf(
        "\t %d contains took %lf [musecs] (avg. among %d "
        "runs)\n",
        num_queries, total_usecs / (runs - 1), runs - 1);
    printf(
        "\t %lf [musecs] per contains (avg. among %d "
        "queries)\n",
        total_usecs / (runs - 1) / num_queries, num_queries);

    for (uint32_t i = 0; i != num_bitmaps; ++i) {
        roaring_bitmap_free(bitmaps[i]);
        bitmaps[i] = NULL;  // paranoid
    }
    free(bitmaps);

    if (munmap((char *)ptr - st.st_size, st.st_size) == -1) {
        printf("unmap failed\n");
        exit(1);
    }

    close(fd);

    return 0;
}