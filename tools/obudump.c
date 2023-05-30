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

/*
 * Currently just an ad-hoc IVF parser to feed packets into obuparse to
 * help spot-check APIs.
 */

#ifdef _WIN32
#define fseeko _fseeki64
#define ftello _fseeki64
#define off_t __int64
#else
#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE
#endif

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "obuparse.h"
#include "tools/json.h"

int main(int argc, char *argv[])
{
    FILE *ivf             = NULL;
    int packet_count      = 0;
    int ret               = 0;
    OBPSequenceHeader hdr = { 0 };
    OBPState state        = { 0 };
    int seen_seq          = 0;

    if (argc < 2) {
        printf("Usage: %s file.ivf\n", argv[0]);
        return 1;
    }

    ivf = fopen(argv[1], "rb");
    if (ivf == NULL) {
        printf("Couldn't open '%s'.\n", argv[1]);
        ret = 1;
        goto end;
    }

    /* Skip IVF global header. */
    ret = fseeko(ivf, 32, SEEK_SET);
    if (ret != 0) {
        printf("Failed to seek past IVF header.\n");
        ret = 1;
        goto end;
    }

    while (!feof(ivf))
    {
        uint8_t frame_header[12];
        uint8_t *packet_buf;
        size_t packet_size;
        size_t packet_pos = 0;
        OBPFrameHeader frame_hdr = {0};
        int SeenFrameHeader = 0;

        size_t read_in = fread(&frame_header[0], 1, 12, ivf);
        if (read_in != 12) {
            if (feof(ivf))
                break;
            printf("Failed to read in IVF frame header (read %zu)\n", read_in);
            ret = 1;
            goto end;
        }

        assert(sizeof(packet_size) >= 4);

        packet_size =  frame_header[0]        +
                      (frame_header[1] << 8)  +
                      (frame_header[2] << 16) +
                      (frame_header[3] << 24);

        printf("{\"packet_number\": %d, \"packet_size\": %zu}\n", packet_count, packet_size);

        packet_buf = malloc(packet_size);
        if (packet_buf == NULL) {
            printf("Could not allocate packet buffer.\n");
            ret = 1;
            goto end;
        }

        read_in = fread(packet_buf, 1, packet_size, ivf);
        if (read_in != packet_size) {
            free(packet_buf);
            printf("Could not read in packet (read %zu)\n", read_in);
            ret = 1;
            goto end;
        }
        packet_count++;

        while (packet_pos < packet_size)
        {
            char err_buf[1024];
            ptrdiff_t offset;
            size_t obu_size;
            int temporal_id, spatial_id;
            OBPOBUType obu_type;
            OBPError err = { &err_buf[0], 1024 };

            ret = obp_get_next_obu(packet_buf + packet_pos, packet_size - packet_pos, 
                                   &obu_type, &offset, &obu_size, &temporal_id, &spatial_id, &err);
            if (ret < 0) {
                free(packet_buf);
                printf("Failed to parse OBU header: %s\n", err.error);
                ret = 1;
                goto end;
            }

            printf("{\"obu_type\": %d, \"offset\": %td, \"obu_size\": %zu, \"temporal_id\": %d, \"spatial_id\": %d}\n",
                   obu_type, offset, obu_size, temporal_id, spatial_id);

            switch (obu_type) {
            case OBP_OBU_TEMPORAL_DELIMITER: {
                assert(obu_size == 0);
                SeenFrameHeader = 0;
                break;
            }
            case OBP_OBU_SEQUENCE_HEADER: {
                seen_seq = 1;
                memset(&hdr, 0, sizeof(hdr));
                ret = obp_parse_sequence_header(packet_buf + packet_pos + offset, obu_size, &hdr, &err);
                if (ret < 0) {
                    free(packet_buf);
                    printf("Failed to parse sequence header: %s\n", err.error);
                    ret = 1;
                    goto end;
                }
                print_json_sequence_header(&hdr);
                break;
            }
            case OBP_OBU_FRAME: {
                OBPTileGroup tiles = { 0 };
                memset(&frame_hdr, 0, sizeof(frame_hdr));
                if (!seen_seq) {
                    free(packet_buf);
                    printf("Encountered Frame Header OBU before Sequence Header OBU.\n");
                    ret = 1;
                    goto end;
                }
                ret = obp_parse_frame(packet_buf + packet_pos + offset, obu_size, &hdr, &state, temporal_id, spatial_id, &frame_hdr, &tiles, &SeenFrameHeader, &err);
                if (ret < 0) {
                    free(packet_buf);
                    printf("Failed to parse frame header: %s\n", err.error);
                    ret = 1;
                    goto end;
                }
                print_json_frame_header(&frame_hdr);
                print_json_tile_group(&tiles);
                break;
            }
            case OBP_OBU_REDUNDANT_FRAME_HEADER:
            case OBP_OBU_FRAME_HEADER: {
                memset(&frame_hdr, 0, sizeof(frame_hdr));
                if (!seen_seq) {
                    free(packet_buf);
                    printf("Encountered Frame Header OBU before Sequence Header OBU.\n");
                    ret = 1;
                    goto end;
                }
                ret = obp_parse_frame_header(packet_buf + packet_pos + offset, obu_size, &hdr, &state, temporal_id, spatial_id, &frame_hdr, &SeenFrameHeader, &err);
                if (ret < 0) {
                    free(packet_buf);
                    printf("Failed to parse frame header: %s\n", err.error);
                    ret = 1;
                    goto end;
                }
                print_json_frame_header(&frame_hdr);
                break;
            }
            case OBP_OBU_TILE_LIST: {
                OBPTileList tile_list = { 0 };
                ret = obp_parse_tile_list(packet_buf + packet_pos + offset, obu_size, &tile_list, &err);
                if (ret < 0) {
                    free(packet_buf);
                    printf("Failed to parse metadata: %s\n", err.error);
                    ret = 1;
                    goto end;
                }
                print_json_tile_list(&tile_list);
                break;
            }
            case OBP_OBU_TILE_GROUP: {
                OBPTileGroup tiles = { 0 };
                ret = obp_parse_tile_group(packet_buf + packet_pos + offset, obu_size, &frame_hdr, &tiles, &SeenFrameHeader, &err);
                if (ret < 0) {
                    free(packet_buf);
                    printf("Failed to parse tile group: %s\n", err.error);
                    ret = 1;
                    goto end;
                }
                print_json_tile_group(&tiles);
                break;
            }
            case OBP_OBU_METADATA: {
                OBPMetadata meta = { 0 };
                ret = obp_parse_metadata(packet_buf + packet_pos + offset, obu_size, &meta, &err);
                if (ret < 0) {
                    free(packet_buf);
                    printf("Failed to parse metadata: %s\n", err.error);
                    ret = 1;
                    goto end;
                }
                print_json_metadata(&meta);
                break;
            }
            default:
                break;
            }

            packet_pos += obu_size + (size_t) offset;
        }

        free(packet_buf);

        if (packet_pos != packet_size) {
            printf("Didn't consume whole packet (%zu vs %zu).\n", packet_size, packet_pos);
            ret = 1;
            goto end;
        }
    }

end:
    if (ivf != NULL)
        fclose(ivf);

    return ret;
}
