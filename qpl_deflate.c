#include "qpl_deflate.h"
#include <stdlib.h>
#include <stdio.h>

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
    if(src_len <= 0) {
        return 0;
    }
    
    qpl_job *job = stream->job;

    job->op            = qpl_op_compress;
    job->level         = qpl_default_level;
    job->next_in_ptr   = (uint8_t *)src;
    job->next_out_ptr  = (uint8_t *)dst;
    job->available_in  = src_len;
    job->available_out = dst_capacity;

    job->flags = QPL_FLAG_FIRST | QPL_FLAG_LAST |
                 QPL_FLAG_DYNAMIC_HUFFMAN |
                 QPL_FLAG_OMIT_VERIFY;// |
   //              QPL_FLAG_GZIP_MODE;

    qpl_status status = qpl_execute_job(job);
    if (status != QPL_STS_OK) {
        printf("qpl_execute_job status = %d\n", status);
        return -1;
    }
    printf("qpl_execute_job status = OK\n");
    *compressed_size = job->total_out;
    return 0;
}

void qpl_deflate_end(qpl_deflate_stream *stream) {
    if (stream->job) qpl_fini_job(stream->job);
    free(stream->job_buffer);
    stream->job = NULL;
    stream->job_buffer = NULL;
}