#include "config.h"
#include "qpl/qpl.h"
#include <stdlib.h>
#include <stdio.h>

qpl_huffman_table_t table = NULL;
allocator_t alloc = {malloc, free};

// Create table
qpl_deflate_huffman_table_create(compression_table_type, qpl_path_software, alloc, &table);

// Build histogram from dataset
qpl_histogram hist = {0};
qpl_gather_deflate_statistics(full_dataset, full_size, &hist, qpl_path_software, 0);

// Initialize table with that histogram
qpl_huffman_table_init_with_histogram(table, &hist);

// Get serialized size and allocate buffer
uint32_t serialized_size = 0;
qpl_huffman_table_get_serialized_size(table, &serialized_size);

uint8_t *buffer = malloc(serialized_size);

// Serialize to buffer
qpl_huffman_table_serialize(table, buffer, serialized_size);

// Save to file
FILE *f = fopen("SAM-Table.bin", "wb");
fwrite(buffer, 1, serialized_size, f);
fclose(f);

free(buffer);
qpl_huffman_table_destroy(table);