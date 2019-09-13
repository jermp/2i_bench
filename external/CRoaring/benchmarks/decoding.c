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

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s binary_filename\n", argv[0]);
        return 1;
    }

    char const *binary_filename = argv[1];

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

    uint32_t *out = (uint32_t *)malloc(universe * sizeof(uint32_t));
    printf("decoding...\n");
    size_t integers = 0;

    double start = get_time_usecs();
    for (uint32_t i = 0; i != num_bitmaps; ++i) {
        size_t size = _roaring_bitmap_to_uint32_array(bitmaps[i], out);
        integers += size;
    }
    double end = get_time_usecs();
    double elapsed = end - start;

    printf("decoded %zu integers\n", integers);

    printf("\t %lf [ns] per integer\n", elapsed / integers * 1000);

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