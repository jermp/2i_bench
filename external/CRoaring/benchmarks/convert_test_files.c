#include <roaring/roaring.h>
#include "benchmark.h"
#include "numbersfromtextfiles.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: %s dirname output_filename ", argv[0]);
        return 1;
    }

    const char *extension = ".txt";
    char const *dirname = argv[1];  // e.g. benchmark/realdata/census-income
    char const *output_filename = argv[2];

    size_t num_sets = 0;
    size_t *sizes = NULL;
    uint32_t **numbers =
        read_all_integer_files(dirname, extension, &sizes, &num_sets);

    for (uint32_t i = 0; i != num_sets; ++i) {
        char *filename = malloc(100 + 1);
        char str[10];
        sprintf(str, "%d", i);
        strcpy(filename, output_filename);
        strcat(filename, str);
        strcat(filename, extension);
        // printf("%s\n", filename);
        FILE *out = fopen(filename, "w");

        fprintf(out, "%zu\n", sizes[i]);
        for (size_t j = 0; j != sizes[i]; ++j) {
            fprintf(out, "%d", numbers[i][j]);
            if (j != sizes[i] - 1) {
                fprintf(out, "\n");
            }
        }
        fclose(out);
    }

    return 0;
}