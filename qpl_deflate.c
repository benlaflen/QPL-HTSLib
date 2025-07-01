#include "qpl_deflate.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int qpl_deflate_init(qpl_deflate_stream *stream) {
    qpl_status status = qpl_get_job_size(qpl_path_software, &stream->job_size);
    if (status != QPL_STS_OK) return -1;

    stream->job_buffer = malloc(stream->job_size);
    if (!stream->job_buffer) return -1;

    stream->job = (qpl_job *)stream->job_buffer;
    status = qpl_init_job(qpl_path_software, stream->job);
    if (status != QPL_STS_OK) return -1;

    return 0;
}

int qpl_deflate_run(qpl_deflate_stream *stream,
                    const void *src, size_t src_len,
                    void *dst, size_t dst_capacity,
                    size_t *compressed_size) {
    if (src_len <= 0) {
        *compressed_size = 0;
        return 0;
    }

    // Allocate aligned input buffer if needed
    void *aligned_src = (void *)src;
    int src_needs_free = 0;
    if (((uintptr_t)src & 63) != 0) {
        if (posix_memalign(&aligned_src, 64, src_len) != 0) return -1;
        memcpy(aligned_src, src, src_len);
        src_needs_free = 1;
    }

    // Always allocate aligned output buffer
    void *aligned_dst = NULL;
    if (posix_memalign(&aligned_dst, 64, dst_capacity) != 0) {
        if (src_needs_free) free(aligned_src);
        return -1;
    }

    qpl_job *job = stream->job;
    job->op            = qpl_op_compress;
    job->level         = qpl_default_level;
    job->next_in_ptr   = aligned_src;
    job->next_out_ptr  = aligned_dst;
    job->available_in  = src_len;
    job->available_out = dst_capacity;

    job->flags = QPL_FLAG_FIRST | QPL_FLAG_LAST | QPL_FLAG_DYNAMIC_HUFFMAN;

    printf("in: %p (%zu), out: %p (%zu), level: %d, flags: 0x%x\n",
           aligned_src, src_len, aligned_dst, dst_capacity, job->level, job->flags);

    qpl_status status = qpl_execute_job(job);

    if (src_needs_free) free(aligned_src);

    if (status != QPL_STS_OK) {
        printf("qpl_execute_job status = %d\n", status);
        free(aligned_dst);
        return -1;
    }

    memcpy(dst, aligned_dst, job->total_out);
    *compressed_size = job->total_out;
    free(aligned_dst);
    return 0;
}

void qpl_deflate_end(qpl_deflate_stream *stream) {
    if (stream->job) qpl_fini_job(stream->job);
    free(stream->job_buffer);
    stream->job = NULL;
    stream->job_buffer = NULL;
}