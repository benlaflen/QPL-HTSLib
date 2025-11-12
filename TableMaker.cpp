#include "qpl/qpl.h"
#include "qpl/c_api/statistics.h"
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cstring>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <input-file>\n", argv[0]);
        return 1;
    }

    // --- Read dataset ---
    FILE *in = fopen(argv[1], "rb");
    if (!in) {
        perror("fopen");
        return 1;
    }
    fseek(in, 0, SEEK_END);
    size_t file_size = ftell(in);
    rewind(in);

    uint8_t *data = static_cast<uint8_t *>(malloc(file_size));
    if (!data) {
        perror("malloc");
        fclose(in);
        return 1;
    }
    if (fread(data, 1, file_size, in) != file_size) {
        fprintf(stderr, "Warning: fread truncated\n");
    }
    fclose(in);

    // --- Create table ---
    allocator_t alloc = {malloc, free};
    qpl_huffman_table_t table = nullptr;

    qpl_status status = qpl_deflate_huffman_table_create(compression_table_type,
                                                         qpl_path_software,
                                                         alloc,
                                                         &table);
    if (status != QPL_STS_OK) {
        printf("Failed to create table: %d\n", status);
        free(data);
        return status;
    }

    // --- Build histogram ---
    qpl_histogram hist = {}; // Zero-initialize safely
    status = qpl_gather_deflate_statistics(data,
                                           static_cast<uint32_t>(file_size),
                                           &hist,
                                           qpl_default_level,  // compression level
                                           qpl_path_software); // execution path
    if (status != QPL_STS_OK) {
        printf("Failed to gather stats: %d\n", status);
        qpl_huffman_table_destroy(table);
        free(data);
        return status;
    }

    // --- Initialize table from histogram ---
    status = qpl_huffman_table_init_with_histogram(table, &hist);
    if (status != QPL_STS_OK) {
        printf("Failed to init table: %d\n", status);
        qpl_huffman_table_destroy(table);
        free(data);
        return status;
    }

    // --- Serialize ---
    serialization_options_t opts = {};
    opts.format = qpl_serialization_format_internal;  // Default internal binary format
    opts.flags = 0;

    size_t serialized_size = 0;
    status = qpl_huffman_table_get_serialized_size(table, opts, &serialized_size);
    if (status != QPL_STS_OK) {
        printf("Failed to get serialized size: %d\n", status);
        qpl_huffman_table_destroy(table);
        free(data);
        return status;
    }

    uint8_t *buffer = static_cast<uint8_t *>(malloc(serialized_size));
    if (!buffer) {
        perror("malloc");
        qpl_huffman_table_destroy(table);
        free(data);
        return 1;
    }

    status = qpl_huffman_table_serialize(table, buffer, serialized_size, opts);
    if (status != QPL_STS_OK) {
        printf("Serialization failed: %d\n", status);
        qpl_huffman_table_destroy(table);
        free(buffer);
        free(data);
        return status;
    }

    FILE *out = fopen("SAM-Table.bin", "wb");
    if (!out) {
        perror("fopen");
        qpl_huffman_table_destroy(table);
        free(buffer);
        free(data);
        return 1;
    }
    fwrite(buffer, 1, serialized_size, out);
    fclose(out);

    printf("Wrote Huffman table (%zu bytes) to SAM-Table.bin\n", serialized_size);

    free(buffer);
    qpl_huffman_table_destroy(table);
    free(data);
    return 0;
}
