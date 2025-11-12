#include "qpl/qpl.h"
#include "qpl/c_api/statistics.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <input-file>\n", argv[0]);
        return 1;
    }

    // --- Read input dataset ---
    FILE *in = fopen(argv[1], "rb");
    if (!in) {
        perror("fopen");
        return 1;
    }
    fseek(in, 0, SEEK_END);
    size_t file_size = ftell(in);
    rewind(in);

    uint8_t *data = malloc(file_size);
    fread(data, 1, file_size, in);
    fclose(in);

    // --- Create Deflate Huffman table ---
    allocator_t alloc = {malloc, free};
    qpl_huffman_table_t table = NULL;

    qpl_status status = qpl_deflate_huffman_table_create(compression_table_type,
                                                         qpl_path_software,
                                                         alloc,
                                                         &table);
    if (status != QPL_STS_OK) {
        printf("Failed to create table: %d\n", status);
        return status;
    }

    // --- Build histogram ---
    qpl_histogram hist = {0};
    status = qpl_gather_deflate_statistics(data, (uint32_t)file_size,
                                           &hist, qpl_path_software, 0);
    if (status != QPL_STS_OK) {
        printf("Failed to gather stats: %d\n", status);
        return status;
    }

    // --- Initialize table from histogram ---
    status = qpl_huffman_table_init_with_histogram(table, &hist);
    if (status != QPL_STS_OK) {
        printf("Failed to init table: %d\n", status);
        return status;
    }

    // --- Serialize to file ---
    uint32_t serialized_size = 0;
    qpl_huffman_table_get_serialized_size(table, &serialized_size);
    uint8_t *buffer = malloc(serialized_size);

    // The serialization API now needs an extra options struct
    serialization_options_t opts = {0};
    status = qpl_huffman_table_serialize(table, buffer, serialized_size, opts);
    if (status != QPL_STS_OK) {
        printf("Serialization failed: %d\n", status);
        return status;
    }

    FILE *out = fopen("SAM-Table.bin", "wb");
    fwrite(buffer, 1, serialized_size, out);
    fclose(out);

    printf("Wrote Huffman table (%u bytes) to SAM-Table.bin\n", serialized_size);

    // --- Cleanup ---
    free(buffer);
    qpl_huffman_table_destroy(table);
    free(data);

    return 0;
}
