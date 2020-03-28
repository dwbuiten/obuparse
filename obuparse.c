/*
 * Copyright (c) 2020, Derek Buitenhuis
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "obuparse.h"

/*********************
 * Helper functions. *
 *********************/

static inline int _obp_is_valid_obu(OBPOBUType type)
{
    return type == OBP_OBU_SEQUENCE_HEADER ||
           type == OBP_OBU_TEMPORAL_DELIMITER ||
           type == OBP_OBU_FRAME_HEADER ||
           type == OBP_OBU_TILE_GROUP ||
           type == OBP_OBU_METADATA ||
           type == OBP_OBU_FRAME ||
           type == OBP_OBU_REDUNDANT_FRAME_HEADER ||
           type == OBP_OBU_TILE_LIST ||
           type == OBP_OBU_PADDING;
}

/************************************
 * Functions from AV1 spcification. *
 ************************************/

static inline int _obp_leb128(uint8_t *buf, size_t size, uint64_t *value, ptrdiff_t *consumed, OBPError *err)
{
    *value       = 0;
    *consumed    = 0;

    for (uint64_t i = 0; i < 8; i++) {
        uint8_t b;

        if (((size_t) (*consumed) + 1) > size) {
            snprintf(err->error, err->size, "Buffer too short to read leb128 value.");
            return -1;
        }

        b       = buf[*consumed];
        *value |= ((uint64_t)(b & 0x7F)) << (i * 7);
        (*consumed)++;

        if ((b & 0x80) != 0x80)
            break;
    }

    return 0;
}

/*****************************
 * API functions start here. *
 *****************************/

int obp_get_next_obu(uint8_t *buf, size_t buf_size, OBPOBUType *obu_type, ptrdiff_t *offset,
                     size_t *size, int *temporal_id, int *spatial_id, OBPError *err)
{
    ptrdiff_t pos = 0;
    int obu_extension_flag;
    int obu_has_size_field;

    if (buf_size < 1) {
        snprintf(err->error, err->size, "Buffer is too small to contain an OBU.");
        return -1;
    }

    *obu_type          = (buf[pos] & 0x78) >> 3;
    obu_extension_flag = (buf[pos] & 0x04) >> 2;
    obu_has_size_field = (buf[pos] & 0x02) >> 1;
    pos++;

    if (!_obp_is_valid_obu(*obu_type)) {
        snprintf(err->error, err->size, "OBU header contains invalid OBU type: %d", *obu_type);
        return -1;
    }

    if (obu_extension_flag) {
        if (buf_size < 1) {
            snprintf(err->error, err->size, "Buffer is too small to contain an OBU extension header.");
            return -1;
        }
        *temporal_id = (buf[pos] & 0xE0) >> 5;
        *spatial_id  = (buf[pos] & 0x18) >> 3;
        pos++;
    } else {
        *temporal_id = 0;
        *spatial_id  = 0;
    }

    if (obu_has_size_field) {
        char err_buf[1024];
        uint64_t value;
        ptrdiff_t consumed;
        OBPError error = { &err_buf[0], 1024 };

        int ret      = _obp_leb128(buf + pos, buf_size - (size_t) pos, &value, &consumed, &error);
        if (ret < 0) {
            snprintf(err->error, err->size, "Failed to read OBU size: %s", &error.error[0]);
            return -1;
        }

        assert(value < UINT32_MAX);

        *offset = (ptrdiff_t) pos + consumed;
        *size   = (size_t) value;
    } else {
        *offset = (ptrdiff_t) pos;
        *size   = buf_size - (size_t) pos;
    }

    if (*size > buf_size - (size_t) offset) {
        snprintf(err->error, err->size, "Invalid OBU size: larger than remaining buffer.");
        return -1;
    }

    return 0;
}
