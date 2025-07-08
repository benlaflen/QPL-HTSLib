#include "qpl_deflate.h"
#include <stdlib.h>
#include <stdio.h>

#define GZIP_ID1 0x1F
#define GZIP_ID2 0x8B
#define GZIP_CM_DEFLATE 0x08

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
        *compressed_size = 0;
        return 0;
    }

    qpl_job *job = stream->job;

    job->op            = qpl_op_compress;
    job->level         = qpl_default_level;
    job->next_in_ptr   = (uint8_t *)src;
    job->next_out_ptr  = (uint8_t *)dst;
    job->available_in  = src_len;
    job->available_out = dst_capacity;

    job->flags = QPL_FLAG_FIRST | QPL_FLAG_LAST | QPL_FLAG_OMIT_VERIFY |
                 QPL_FLAG_DYNAMIC_HUFFMAN;// |
   //              QPL_FLAG_GEN_LITERALS;QPL_FLAG_OMIT_VERIFY;// |
   //              QPL_FLAG_GZIP_MODE;

    qpl_status status = qpl_execute_job(job);
    if (status != QPL_STS_OK) {
        printf("qpl_execute_job status = %d\n", status);
        return -1;
    }
    *compressed_size = job->total_out;
    return 0;
}

int unwrap_deflate_stream(const uint8_t *src, size_t slen,
                          const uint8_t **out_deflate, size_t *out_len) {
    if (slen < 2) return -1;

    // --- Check for GZIP ---
    if (src[0] == GZIP_ID1 && src[1] == GZIP_ID2 && src[2] == GZIP_CM_DEFLATE) {
        if (slen < 10) return -1;

        size_t offset = 10;  // skip fixed 10-byte header
        uint8_t flg = src[3];

        // FLG bits: https://datatracker.ietf.org/doc/html/rfc1952#section-2.3.1
        if (flg & 0x04) { // FEXTRA
            if (offset + 2 > slen) return -1;
            uint16_t xlen = src[offset] | (src[offset+1] << 8);
            offset += 2 + xlen;
            if (offset > slen) return -1;
        }
        if (flg & 0x08) { // FNAME
            while (offset < slen && src[offset] != 0) offset++;
            offset++;
        }
        if (flg & 0x10) { // FCOMMENT
            while (offset < slen && src[offset] != 0) offset++;
            offset++;
        }
        if (flg & 0x02) { // FHCRC
            offset += 2;
        }

        if (offset >= slen || slen < offset + 8) return -1; // need space for DEFLATE + footer

        *out_deflate = src + offset;
        *out_len = slen - offset - 8; // exclude CRC32 + ISIZE
        return 0;
    }

    // --- Check for zlib ---
    if ((src[0] & 0x0F) == 0x08 && ((src[0] << 8) | src[1]) % 31 == 0) {
        // Basic zlib header (CMF + FLG), RFC1950
        if (slen < 6) return -1; // 2 header + 4 footer
        *out_deflate = src + 2;
        *out_len = slen - 6; // omit adler32
        return 0;
    }

    // --- Assume raw DEFLATE ---
    *out_deflate = src;
    *out_len = slen;
    return 0;
}

int qpl_inflate_run(qpl_deflate_stream *stream,
                    const void *src, size_t src_len,
                    void *dst, size_t dst_capacity,
                    size_t *decompressed_size) {
    if (src_len == 0) {
        *decompressed_size = 0;
        return 0;
    }

    qpl_job *job = stream->job;

    job->op            = qpl_op_decompress;
    job->next_in_ptr   = (uint8_t *)src;
    job->next_out_ptr  = (uint8_t *)dst;
    job->available_in  = src_len;
    job->available_out = dst_capacity;

    job->flags = QPL_FLAG_FIRST | QPL_FLAG_LAST |
                 QPL_FLAG_OMIT_VERIFY;  // Omit verify = faster, you already do CRC manually

    // Possibly required depending on QPL version:
    // job->decomp_end_processing_hint = qpl_decomp_end_processing_complete;

    qpl_status status = qpl_execute_job(job);
    if (status != QPL_STS_OK) {
        printf("qpl_execute_job (inflate) status = %d\n", status);
        return -1;
    }

    *decompressed_size = job->total_out;
    return 0;
}

void qpl_deflate_end(qpl_deflate_stream *stream) {
    if (stream->job) qpl_fini_job(stream->job);
    free(stream->job_buffer);
    stream->job = NULL;
    stream->job_buffer = NULL;
}