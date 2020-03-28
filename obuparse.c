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
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "obuparse.h"

/************************************
 * Bitreader functions and structs. *
 ************************************/

typedef struct _OBPBitReader {
    uint8_t *buf;
    size_t buf_size;
    size_t buf_pos;
    uint64_t bit_buffer;
    uint8_t bits_in_buf;
} _OBPBitReader;

static inline _OBPBitReader _obp_new_br(uint8_t *buf, size_t buf_size)
{
    _OBPBitReader ret = { buf, buf_size, 0, 0, 0 };
    return ret;
}

static inline uint64_t _obp_br_unchecked(_OBPBitReader *br, uint8_t n)
{
    assert(n <= 63);

    while (n > br->bits_in_buf) {
        br->bit_buffer <<= 8;
        br->bit_buffer  |= (uint64_t) br->buf[br->buf_pos];
        br->bits_in_buf += 8;
        br->buf_pos++;

        if (br->bits_in_buf > 56) {
            if (n <= br->bits_in_buf)
                break;

            if (n <= 64)
                return (_obp_br_unchecked(br, 32) << 32) | (_obp_br_unchecked(br, n - 32));
        }
    }

    br->bits_in_buf -= n;
    return (br->bit_buffer >> br->bits_in_buf) & ((((uint64_t)1) << n) - 1);
}

#if OBP_UNCHECKED_BITREADER
#define _obp_br(x, br, n) do { \
    x = _obp_br_unchecked(br, n); \
} while(0)
#else
#define _obp_br(x, br, n) do { \
    size_t bytes_needed = ((n - br->bits_in_buf) + (1<<3) - 1) >> 3; \
    if (bytes_needed > (br->buf_size - br->buf_pos)) { \
        snprintf(err->error, err->size, "Ran out of bytes in buffer."); \
        return -1; \
    } \
    x = _obp_br_unchecked(br, n); \
} while(0)
#endif

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

static inline int _obp_uvlc(_OBPBitReader *br, uint32_t *value, OBPError *err)
{
    uint32_t leading_zeroes = 0;
    while (leading_zeroes < 32) {
        int b;
        _obp_br(b, br, 1);
        if (b != 0)
            break;
        leading_zeroes++;
    }
    if (leading_zeroes == 32) {
        snprintf(err->error, err->size, "Invalid VLC.");
        return -1;
    }
    uint32_t val;
    _obp_br(val, br, leading_zeroes);
    *value = val + ((1 << leading_zeroes) - 1);
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

int obp_parse_sequence_header(uint8_t *buf, size_t buf_size, OBPSequenceHeader *seq_header, OBPError *err)
{
    _OBPBitReader b   = _obp_new_br(buf, buf_size);
    _OBPBitReader *br = &b;

    _obp_br(seq_header->seq_profile, br, 3);
    _obp_br(seq_header->still_picture, br, 1);
    _obp_br(seq_header->reduced_still_picture_header, br, 1);
    if (seq_header->reduced_still_picture_header) {
        seq_header->timing_info_present_flag                     = 0;
        seq_header->decoder_model_info_present_flag              = 0;
        seq_header->initial_display_delay_present_flag           = 0;
        seq_header->operating_points_cnt_minus_1                 = 0;
        seq_header->operating_point_idc[0]                       = 0;
        seq_header->seq_level_idx[0]                             = 0;
        seq_header->seq_tier[0]                                  = 0;
        seq_header->decoder_model_present_for_this_op[0]         = 0;
        seq_header->initial_display_delay_present_for_this_op[0] = 0;
    } else {
        _obp_br(seq_header->timing_info_present_flag, br, 1);
        if (seq_header->timing_info_present_flag) {
            /* timing_info() */
            _obp_br(seq_header->timing_info.num_units_in_display_tick, br, 32);
            _obp_br(seq_header->timing_info.time_scale, br, 32);
            _obp_br(seq_header->timing_info.equal_picture_interval, br, 1);
            if (seq_header->timing_info.equal_picture_interval) {
                int ret = _obp_uvlc(br, &seq_header->timing_info.num_ticks_per_picture_minus_1, err);
                if (ret < 0)
                    return -1;
            }
            _obp_br(seq_header->decoder_model_info_present_flag, br, 1);
            if (seq_header->decoder_model_info_present_flag) {
                /* decoder_model_info() */
                _obp_br(seq_header->decoder_model_info.buffer_delay_length_minus_1, br, 5);
                _obp_br(seq_header->decoder_model_info.num_units_in_decoding_tick, br, 32);
                _obp_br(seq_header->decoder_model_info.buffer_removal_time_length_minus_1, br, 5);
                _obp_br(seq_header->decoder_model_info.frame_presentation_time_length_minus_1, br, 5);
            }
        } else {
            seq_header->decoder_model_info_present_flag = 0;
        }
        _obp_br(seq_header->initial_display_delay_present_flag, br, 1);
        _obp_br(seq_header->operating_points_cnt_minus_1, br, 5);
        for (uint8_t i = 0; i <= seq_header->operating_points_cnt_minus_1; i++) {
            _obp_br(seq_header->operating_point_idc[i], br, 12);
            _obp_br(seq_header->seq_level_idx[i], br, 5);
            if (seq_header->seq_level_idx[i] > 7) {
                _obp_br(seq_header->seq_tier[i], br, 1);
            } else {
                seq_header->seq_tier[i] = 0;
            }
            if (seq_header->decoder_model_info_present_flag) {
                _obp_br(seq_header->decoder_model_present_for_this_op[i], br, 1);
                if (seq_header->decoder_model_present_for_this_op[i]) {
                    /* operating_parameters_info() */
                    uint8_t n = seq_header->decoder_model_info.buffer_delay_length_minus_1 + 1;
                    _obp_br(seq_header->operating_parameters_info[i].decoder_buffer_delay, br, n);
                    _obp_br(seq_header->operating_parameters_info[i].encoder_buffer_delay, br, n);
                    _obp_br(seq_header->operating_parameters_info[i].low_delay_mode_flag, br, 1);
                }
            } else {
                seq_header->decoder_model_present_for_this_op[i] = 0;
            }
            if (seq_header->initial_display_delay_present_flag) {
                _obp_br(seq_header->initial_display_delay_present_for_this_op[i], br, 1);
                if (seq_header->initial_display_delay_present_for_this_op[i]) {
                    _obp_br(seq_header->initial_display_delay_minus_1[i], br, 4);
                }
            }
        }
    }
    _obp_br(seq_header->frame_width_bits_minus_1, br, 4);
    _obp_br(seq_header->frame_height_bits_minus_1, br, 4);
    _obp_br(seq_header->max_frame_width_minus_1, br, seq_header->frame_width_bits_minus_1 + 1);
    _obp_br(seq_header->max_frame_height_minus_1, br, seq_header->frame_height_bits_minus_1 + 1);
    if (seq_header->reduced_still_picture_header) {
        seq_header->frame_id_numbers_present_flag = 0;
    } else {
        _obp_br(seq_header->frame_id_numbers_present_flag, br, 1);
    }
    if (seq_header->frame_id_numbers_present_flag) {
        _obp_br(seq_header->delta_frame_id_length_minus_2, br, 4);
        _obp_br(seq_header->additional_frame_id_length_minus_1, br, 3);
    }
    _obp_br(seq_header->use_128x128_superblock, br, 1);
    _obp_br(seq_header->enable_filter_intra, br, 1);
    _obp_br(seq_header->enable_intra_edge_filter, br, 1);
    if (seq_header->reduced_still_picture_header) {
        seq_header->enable_interintra_compound     = 0;
        seq_header->enable_masked_compound         = 0;
        seq_header->enable_warped_motion           = 0;
        seq_header->enable_dual_filter             = 0;
        seq_header->enable_order_hint              = 0;
        seq_header->enable_jnt_comp                = 0;
        seq_header->enable_ref_frame_mvs           = 0;
        seq_header->seq_force_screen_content_tools = 2; /* SELECT_SCREEN_CONTENT_TOOLS */
        seq_header->seq_force_integer_mv           = 2; /* SELECT_INTEGER_MV */
        seq_header->OrderHintBits                  = 0;
    } else {
        _obp_br(seq_header->enable_interintra_compound, br, 1);
        _obp_br(seq_header->enable_masked_compound, br, 1);
        _obp_br(seq_header->enable_warped_motion, br, 1);
        _obp_br(seq_header->enable_dual_filter, br, 1);
        _obp_br(seq_header->enable_order_hint, br, 1);
        if (seq_header->enable_order_hint) {
            _obp_br(seq_header->enable_jnt_comp, br, 1);
            _obp_br(seq_header->enable_ref_frame_mvs, br, 1);
        } else {
            seq_header->enable_jnt_comp = 0;
            seq_header->enable_ref_frame_mvs = 0;
        }
        _obp_br(seq_header->seq_choose_screen_content_tools, br, 1);
        if (seq_header->seq_choose_screen_content_tools) {
            seq_header->seq_force_screen_content_tools = 2; /* SELECT_SCREEN_CONTENT_TOOLS */
        } else {
            _obp_br(seq_header->seq_force_screen_content_tools, br, 1);
        }
        if (seq_header->seq_force_screen_content_tools > 0) {
            _obp_br(seq_header->seq_choose_integer_mv, br, 1);
            if (seq_header->seq_choose_integer_mv) {
                seq_header->seq_force_integer_mv = 2; /* SELECT_INTEGER_MV */
            } else {
                _obp_br(seq_header->seq_force_integer_mv, br, 1);
            }
        } else {
            seq_header->seq_force_integer_mv = 2; /* SELECT_INTEGER_MV */
        }
        if (seq_header->enable_order_hint) {
            _obp_br(seq_header->order_hint_bits_minus_1, br, 3);
            seq_header->OrderHintBits = seq_header->order_hint_bits_minus_1 + 1;
        } else {
            seq_header->OrderHintBits = 0;
        }
    }
    _obp_br(seq_header->enable_superres, br, 1);
    _obp_br(seq_header->enable_cdef, br, 1);
    _obp_br(seq_header->enable_restoration, br, 1);
    /* color_config() */
    _obp_br(seq_header->color_config.high_bitdepth, br, 1);
    if (seq_header->seq_profile == 2 && seq_header->color_config.high_bitdepth) {
        _obp_br(seq_header->color_config.twelve_bit, br, 1);
        seq_header->color_config.BitDepth = seq_header->color_config.twelve_bit ? 12 : 10;
    } else {
        seq_header->color_config.BitDepth = seq_header->color_config.high_bitdepth ? 10 : 8;
    }
    if (seq_header->seq_profile == 1) {
        seq_header->color_config.mono_chrome = 0;
    } else {
        _obp_br(seq_header->color_config.mono_chrome, br, 1);
    }
    seq_header->color_config.NumPlanes = seq_header->color_config.mono_chrome ? 1 : 3;
    _obp_br(seq_header->color_config.color_description_present_flag, br, 1);
    if (seq_header->color_config.color_description_present_flag) {
        _obp_br(seq_header->color_config.color_primaries, br, 8);
        _obp_br(seq_header->color_config.transfer_characteristics, br, 8);
        _obp_br(seq_header->color_config.matrix_coefficients, br, 8);
    } else {
        seq_header->color_config.color_primaries          = OBP_CP_UNSPECIFIED;
        seq_header->color_config.transfer_characteristics = OBP_TC_UNSPECIFIED;
        seq_header->color_config.matrix_coefficients      = OBP_MC_UNSPECIFIED;
    }
    if (seq_header->color_config.mono_chrome) {
        _obp_br(seq_header->color_config.color_range, br, 1);
        seq_header->color_config.subsampling_x          = 1;
        seq_header->color_config.subsampling_y          = 1;
        seq_header->color_config.chroma_sample_position = OBP_CSP_UNKNOWN;
        seq_header->color_config.separate_uv_delta_q    = 0;
        goto color_done;
    } else if (seq_header->color_config.color_primaries == OBP_CP_BT_709 &&
               seq_header->color_config.transfer_characteristics == OBP_TC_SRGB &&
               seq_header->color_config.matrix_coefficients == OBP_MC_IDENTITY) {
        seq_header->color_config.color_range = 1;
        seq_header->color_config.subsampling_x = 0;
        seq_header->color_config.subsampling_y = 0;
    } else {
        _obp_br(seq_header->color_config.color_range, br, 1);
        if (seq_header->seq_profile == 0) {
            seq_header->color_config.subsampling_x = 1;
            seq_header->color_config.subsampling_y = 1;
        } else if (seq_header->seq_profile == 1) {
            seq_header->color_config.subsampling_x = 0;
            seq_header->color_config.subsampling_y = 0;
        } else {
            if (seq_header->color_config.BitDepth == 12) {
                _obp_br(seq_header->color_config.subsampling_x, br, 1);
                if (seq_header->color_config.subsampling_x) {
                    _obp_br(seq_header->color_config.subsampling_y, br, 1);
                } else {
                    seq_header->color_config.subsampling_y = 0;
                }
            } else {
                seq_header->color_config.subsampling_x = 1;
                seq_header->color_config.subsampling_y = 0;
            }
        }
        if (seq_header->color_config.subsampling_x && seq_header->color_config.subsampling_y) {
            _obp_br(seq_header->color_config.chroma_sample_position, br, 2);
        }
    }
    _obp_br(seq_header->color_config.separate_uv_delta_q, br, 1);

color_done:
    _obp_br(seq_header->film_grain_params_present, br, 1);

    return 0;
}

int obp_parse_metadata(uint8_t *buf, size_t buf_size, OBPMetadata *metadata, OBPError *err)
{
    uint64_t val;
    ptrdiff_t consumed;
    char err_buf[1024];
    _OBPBitReader b;
    _OBPBitReader *br;
    OBPError error = { &err_buf[0], 1024 };

    int ret = _obp_leb128(buf, buf_size, &val, &consumed, &error);
    if (ret < 0) {
        snprintf(err->error, err->size, "Couldn't read metadata type: %s", error.error);
        return -1;
    }
    metadata->metadata_type = val;

    b  = _obp_new_br(buf + consumed, buf_size - consumed);
    br = &b;

    if (metadata->metadata_type == OBP_METADATA_TYPE_HDR_CLL) {
        _obp_br(metadata->metadata_hdr_cll.max_cll, br, 16);
        _obp_br(metadata->metadata_hdr_cll.max_fall, br, 16);
    } else if (metadata->metadata_type == OBP_METADATA_TYPE_HDR_MDCV) {
        for (int i = 0; i < 3; i++) {
            _obp_br(metadata->metadata_hdr_mdcv.primary_chromaticity_x[i], br, 16);
            _obp_br(metadata->metadata_hdr_mdcv.primary_chromaticity_y[i], br, 16);
        }
        _obp_br(metadata->metadata_hdr_mdcv.white_point_chromaticity_x, br, 16);
        _obp_br(metadata->metadata_hdr_mdcv.white_point_chromaticity_y, br, 16);
        _obp_br(metadata->metadata_hdr_mdcv.luminance_max, br, 32);
        _obp_br(metadata->metadata_hdr_mdcv.luminance_min, br, 32);
    } else if (metadata->metadata_type == OBP_METADATA_TYPE_SCALABILITY) {
        _obp_br(metadata->metadata_scalability.scalability_mode_idc, br, 8);
        if (metadata->metadata_scalability.scalability_mode_idc) {
            /* scalability_structure() */
            _obp_br(metadata->metadata_scalability.scalability_structure.spatial_layers_cnt_minus_1, br, 2);
            _obp_br(metadata->metadata_scalability.scalability_structure.spatial_layer_dimensions_present_flag, br, 1);
            _obp_br(metadata->metadata_scalability.scalability_structure.spatial_layer_description_present_flag, br, 1);
            _obp_br(metadata->metadata_scalability.scalability_structure.temporal_group_description_present_flag, br, 1);
            _obp_br(metadata->metadata_scalability.scalability_structure.scalability_structure_reserved_3bits, br, 3);
            if (metadata->metadata_scalability.scalability_structure.spatial_layer_dimensions_present_flag) {
                for (uint8_t i = 0; i < metadata->metadata_scalability.scalability_structure.spatial_layers_cnt_minus_1; i++) {
                    _obp_br(metadata->metadata_scalability.scalability_structure.spatial_layer_max_width[i], br, 16);
                    _obp_br(metadata->metadata_scalability.scalability_structure.spatial_layer_max_height[i], br, 16);
                }
            }
            if (metadata->metadata_scalability.scalability_structure.spatial_layer_description_present_flag) {
                for (uint8_t i = 0; i < metadata->metadata_scalability.scalability_structure.spatial_layers_cnt_minus_1; i++) {
                    _obp_br(metadata->metadata_scalability.scalability_structure.spatial_layer_ref_id[i], br, 8);
                }
            }
            if (metadata->metadata_scalability.scalability_structure.temporal_group_description_present_flag) {
                _obp_br(metadata->metadata_scalability.scalability_structure.temporal_group_size, br, 8);
                for (uint8_t i = 0; i < metadata->metadata_scalability.scalability_structure.temporal_group_size; i++) {
                    _obp_br(metadata->metadata_scalability.scalability_structure.temporal_group_temporal_id[i], br, 3);
                    _obp_br(metadata->metadata_scalability.scalability_structure.temporal_group_temporal_switching_up_point_flag[i], br, 1);
                    _obp_br(metadata->metadata_scalability.scalability_structure.temporal_group_spatial_switching_up_point_flag[i], br, 1);
                    _obp_br(metadata->metadata_scalability.scalability_structure.temporal_group_ref_cnt[i], br, 3);
                    for (uint8_t j = 0; j < metadata->metadata_scalability.scalability_structure.temporal_group_ref_cnt[i]; j++) {
                        _obp_br(metadata->metadata_scalability.scalability_structure.temporal_group_ref_pic_diff[i][j], br, 8);
                    }
                }
            }
        }
    } else if (metadata->metadata_type == OBP_METADATA_TYPE_ITUT_T35) {
        size_t offset = 1;
        _obp_br(metadata->metadata_itut_t35.itu_t_t35_country_code, br, 8);
        if (metadata->metadata_itut_t35.itu_t_t35_country_code == 0xFF) {
            _obp_br(metadata->metadata_itut_t35.itu_t_t35_country_code_extension_byte, br, 8);
            offset++;
        }
        metadata->metadata_itut_t35.itu_t_t35_payload_bytes       = buf + consumed + offset;
        metadata->metadata_itut_t35.itu_t_t35_payload_bytes_size = buf_size - consumed - offset;
    } else if (metadata->metadata_type == OBP_METADATA_TYPE_TIMECODE) {
        _obp_br(metadata->metadata_timecode.counting_type, br, 5);
        _obp_br(metadata->metadata_timecode.full_timestamp_flag, br, 1);
        _obp_br(metadata->metadata_timecode.discontinuity_flag, br, 1);
        _obp_br(metadata->metadata_timecode.cnt_dropped_flag, br, 1);
        _obp_br(metadata->metadata_timecode.n_frames, br, 9);
        if (metadata->metadata_timecode.full_timestamp_flag) {
            _obp_br(metadata->metadata_timecode.seconds_value, br, 6);
            _obp_br(metadata->metadata_timecode.minutes_value, br, 6);
            _obp_br(metadata->metadata_timecode.hours_value, br, 5);
        } else {
            _obp_br(metadata->metadata_timecode.seconds_flag, br, 1);
            if (metadata->metadata_timecode.seconds_flag) {
                _obp_br(metadata->metadata_timecode.seconds_value, br, 6);
                _obp_br(metadata->metadata_timecode.minutes_flag, br, 1);
                if (metadata->metadata_timecode.minutes_flag) {
                    _obp_br(metadata->metadata_timecode.minutes_value, br, 6);
                    _obp_br(metadata->metadata_timecode.hours_flag, br, 1);
                    if (metadata->metadata_timecode.hours_flag) {
                        _obp_br(metadata->metadata_timecode.hours_value, br, 5);
                    }
                }
            }
        }
        _obp_br(metadata->metadata_timecode.time_offset_length, br, 5);
        if (metadata->metadata_timecode.time_offset_length > 0) {
             _obp_br(metadata->metadata_timecode.time_offset_value, br, metadata->metadata_timecode.time_offset_length);
        }
    } else if (metadata->metadata_type >= 6 && metadata->metadata_type <= 31) {
        metadata->unregistered.buf      = buf + consumed;
        metadata->unregistered.buf_size = buf_size - consumed;
    } else {
        snprintf(err->error, err->size, "Invalid metadata type: %"PRIu32"\n", metadata->metadata_type);
        return -1;
    }

    return 0;
}
