#ifndef QPL_DEFLATE_H
#define QPL_DEFLATE_H

#include <stddef.h>
#include <stdint.h>
#include <qpl/qpl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    qpl_job *job;
    uint8_t *job_buffer;
    uint32_t job_size;
} qpl_deflate_stream;

int  qpl_deflate_init(qpl_deflate_stream *stream);
int  qpl_deflate_run(qpl_deflate_stream *stream,
                     const void *src, size_t src_len,
                     void *dst, size_t dst_capacity,
                     size_t *compressed_size);
int qpl_inflate_run(qpl_deflate_stream *stream,
                    const void *src, size_t src_len,
                    void *dst, size_t dst_capacity,
                    size_t *decompressed_size);
void qpl_deflate_end(qpl_deflate_stream *stream);

#ifdef __cplusplus
}
#endif

#endif // QPL_DEFLATE_H