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

#ifndef OBUPARSE_H
#define OBUPARSE_H

#include <stddef.h>
#include <stdint.h>

/*
 * OBU types.
 */
typedef enum {
    /* 0 Reserved */
    OBP_OBU_SEQUENCE_HEADER = 1,
    OBP_OBU_TEMPORAL_DELIMITER = 2,
    OBP_OBU_FRAME_HEADER = 3,
    OBP_OBU_TILE_GROUP = 4,
    OBP_OBU_METADATA = 5,
    OBP_OBU_FRAME = 6,
    OBP_OBU_REDUNDANT_FRAME_HEADER = 7,
    OBP_OBU_TILE_LIST = 8,
    /* 9-14 Reserved */
    OBP_OBU_PADDING = 15
} OBPOBUType;

/*
 * OBPError contains a user-provided buffer and buffer size
 * where obuparse can write error messages to.
 */
typedef struct OBPError {
    char *error;
    size_t size;
} OBPError;

/*
 * obp_get_next_obu parses the next OBU header in a packet containing a set of one or more OBUs
 * (e.g. an IVF or ISOBMFF packet) and returns its location in the buffer, as well as all
 * relevant data from the header.
 *
 * Input:
 *     buf      - Input packet buffer.
 *     buf_size - Size of the input packet buffer.
 *     err      - An error buffer and buffer size to write any error messages into.
 *
 * Output:
 *     obu_type    - The type of OBU.
 *     offset      - The offset into the buffer where this OBU starts, excluding the OBU header.
 *     obu_size    - The size of the OBU, excluding the size of the OBU header.
 *     temporal_id - The temporal ID of the OBU.
 *     spatial_id  - The spatia ID of the OBU.
 *
 * Returns:
 *     0 on success, -1 on error.
 */
int obp_get_next_obu(uint8_t *buf, size_t buf_size, OBPOBUType *obu_type, ptrdiff_t *offset,
                     size_t *obu_size, int *temporal_id, int *spatial_id, OBPError *err);

#endif
