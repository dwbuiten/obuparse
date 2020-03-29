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

/*********************************************
 * Various enums from the AV1 specification. *
 *********************************************/

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
 * Metadata types for the Metadata OBU.
 */
typedef enum {
    /* 0 Reserved */
    OBP_METADATA_TYPE_HDR_CLL = 1,
    OBP_METADATA_TYPE_HDR_MDCV = 2,
    OBP_METADATA_TYPE_SCALABILITY = 3,
    OBP_METADATA_TYPE_ITUT_T35 = 4,
    OBP_METADATA_TYPE_TIMECODE = 5
    /* 6-31 Unregistered user private */
    /* 32 and greater Reserved for AOM use */
} OBPMetadataType;

/*
 * Color primaries.
 *
 * These match ISO/IEC 23091-4/ITU-T H.273.
 */
typedef enum {
    OBP_CP_BT_709 = 1,
    OBP_CP_UNSPECIFIED = 2,
    OBP_CP_BT_470_M = 4,
    OBP_CP_BT_470_B_G = 5,
    OBP_CP_BT_601 = 6,
    OBP_CP_SMPTE_240 = 7,
    OBP_CP_GENERIC_FILM = 8,
    OBP_CP_BT_2020 = 9,
    OBP_CP_XYZ = 10,
    OBP_CP_SMPTE_431 = 11,
    OBP_CP_SMPTE_432 = 12,
    OBP_CP_EBU_3213 = 22
} OBPColorPrimaries;

/*
 * Transfer characteristics.
 *
 * These match ISO/IEC 23091-4/ITU-T H.273.
 */
typedef enum {
    OBP_TC_RESERVED_0 = 0,
    OBP_TC_BT_709 = 1,
    OBP_TC_UNSPECIFIED = 2,
    OBP_TC_RESERVED_3 = 3,
    OBP_TC_BT_470_M = 4,
    OBP_TC_BT_470_B_G = 5,
    OBP_TC_BT_601 = 6,
    OBP_TC_SMPTE_240 = 7,
    OBP_TC_LINEAR = 8,
    OBP_TC_LOG_100 = 9,
    OBP_TC_LOG_100_SQRT10 = 10,
    OBP_TC_IEC_61966 = 11,
    OBP_TC_BT_1361 = 12,
    OBP_TC_SRGB = 13,
    OBP_TC_BT_2020_10_BIT = 14,
    OBP_TC_BT_2020_12_BIT = 15,
    OBP_TC_SMPTE_2084 = 16,
    OBP_TC_SMPTE_428 = 17,
    OBP_TC_HLG = 18
} OBPTransferCharacteristics;

/*
 * Color matrix coefficients.
 *
 * These matchISO/IEC 23091-4/ITU-T H.273.
 */
typedef enum {
    OBP_MC_IDENTITY = 0,
    OBP_MC_BT_709 = 1,
    OBP_MC_UNSPECIFIED = 2,
    OBP_MC_RESERVED_3 = 3,
    OBP_MC_FCC = 4,
    OBP_MC_BT_470_B_G = 5,
    OBP_MC_BT_601 = 6,
    OBP_MC_SMPTE_240 = 7,
    OBP_MC_SMPTE_YCGCO = 8,
    OBP_MC_BT_2020_NCL = 9,
    OBP_MC_BT_2020_CL = 10,
    OBP_MC_SMPTE_2085 = 11,
    OBP_MC_CHROMAT_NCL = 12,
    OBP_MC_CHROMAT_CL = 13,
    OBP_MC_ICTCP = 14
} OBPMatrixCoefficients;

/*
 * Chroma sample position.
 */
typedef enum {
    OBP_CSP_UNKNOWN = 0,
    OBP_CSP_VERTICAL = 1,
    OBP_CSP_COLOCATED = 2
    /* 3 Reserved */
} OBPChromaSamplePosition;

/**************************************************
 * Various structures from the AV1 specification. *
 **************************************************/

/*
 * Sequence Header OBU
 */
typedef struct OBPSequenceHeader {
    uint8_t seq_profile;
    int still_picture;
    int reduced_still_picture_header;
    int timing_info_present_flag;
    struct {
        uint32_t num_units_in_display_tick;
        uint32_t time_scale;
        int equal_picture_interval;
        uint32_t num_ticks_per_picture_minus_1;
    } timing_info;
    int decoder_model_info_present_flag;
    struct {
        uint8_t buffer_delay_length_minus_1;
        uint32_t num_units_in_decoding_tick;
        uint8_t buffer_removal_time_length_minus_1;
        uint8_t frame_presentation_time_length_minus_1;
    } decoder_model_info;
    int initial_display_delay_present_flag;
    uint8_t operating_points_cnt_minus_1;
    uint8_t operating_point_idc[32];
    uint8_t seq_level_idx[32];
    uint8_t seq_tier[32];
    int decoder_model_present_for_this_op[32];
    struct {
        uint64_t decoder_buffer_delay;
        uint64_t encoder_buffer_delay;
        int low_delay_mode_flag;
    } operating_parameters_info[32];
    int initial_display_delay_present_for_this_op[32];
    uint8_t initial_display_delay_minus_1[32];
    uint8_t frame_width_bits_minus_1;
    uint8_t frame_height_bits_minus_1;
    uint32_t max_frame_width_minus_1;
    uint32_t max_frame_height_minus_1;
    int frame_id_numbers_present_flag;
    uint8_t delta_frame_id_length_minus_2;
    uint8_t additional_frame_id_length_minus_1;
    int use_128x128_superblock;
    int enable_filter_intra;
    int enable_intra_edge_filter;
    int enable_interintra_compound;
    int enable_masked_compound;
    int enable_warped_motion;
    int enable_dual_filter;
    int enable_order_hint;
    int enable_jnt_comp;
    int enable_ref_frame_mvs;
    int seq_choose_screen_content_tools;
    int seq_force_screen_content_tools;
    int seq_choose_integer_mv;
    int seq_force_integer_mv;
    uint8_t order_hint_bits_minus_1;
    uint8_t OrderHintBits;
    int enable_superres;
    int enable_cdef;
    int enable_restoration;
    struct {
        int high_bitdepth;
        int twelve_bit;
        uint8_t BitDepth;
        int mono_chrome;
        uint8_t NumPlanes;
        int color_description_present_flag;
        OBPColorPrimaries color_primaries;
        OBPTransferCharacteristics transfer_characteristics;
        OBPMatrixCoefficients matrix_coefficients;
        int color_range;
        int subsampling_x;
        int subsampling_y;
        OBPChromaSamplePosition chroma_sample_position;
        int separate_uv_delta_q;
    } color_config;
    int film_grain_params_present;
} OBPSequenceHeader;

/*
 * Tile List OBU
 */
typedef struct OBPTileList {
    uint8_t output_frame_width_in_tiles_minus_1;
    uint8_t output_frame_height_in_tiles_minus_1;
    uint16_t tile_count_minus_1;
    struct {
        uint8_t anchor_frame_idx;
        uint8_t anchor_tile_row;
        uint8_t anchor_tile_col;
        uint16_t tile_data_size_minus_1;
        uint8_t *coded_tile_data;
        size_t coded_tile_data_size;
    } tile_list_entry[65536];
} OBPTileList;

/*
 * Metadata OBU
 */
typedef struct OBPMetadata {
    OBPMetadataType metadata_type;
    struct {
        uint8_t itu_t_t35_country_code; /* Annex A of Recommendation ITU-T T.35. */
        uint8_t itu_t_t35_country_code_extension_byte;
        uint8_t *itu_t_t35_payload_bytes;
        size_t itu_t_t35_payload_bytes_size;
    } metadata_itut_t35;
    struct {
        uint16_t max_cll;
        uint16_t max_fall;
    } metadata_hdr_cll;
    struct {
        uint16_t primary_chromaticity_x[3];
        uint16_t primary_chromaticity_y[3];
        uint16_t white_point_chromaticity_x;
        uint16_t white_point_chromaticity_y;
        uint32_t luminance_max;
        uint32_t luminance_min;
    } metadata_hdr_mdcv;
    struct {
        uint8_t scalability_mode_idc;
        struct {
            uint8_t spatial_layers_cnt_minus_1;
            int spatial_layer_dimensions_present_flag;
            int spatial_layer_description_present_flag;
            int temporal_group_description_present_flag;
            uint8_t scalability_structure_reserved_3bits;
            uint16_t spatial_layer_max_width[3];
            uint16_t spatial_layer_max_height[3];
            uint8_t spatial_layer_ref_id[3];
            uint8_t temporal_group_size;
            uint8_t temporal_group_temporal_id[256];
            int temporal_group_temporal_switching_up_point_flag[256];
            int temporal_group_spatial_switching_up_point_flag[256];
            uint8_t temporal_group_ref_cnt[256];
            uint8_t temporal_group_ref_pic_diff[256][8];
        } scalability_structure;
    } metadata_scalability;
    struct {
        uint8_t counting_type;
        int full_timestamp_flag;
        int discontinuity_flag;
        int cnt_dropped_flag;
        uint16_t n_frames;
        uint8_t seconds_value;
        uint8_t minutes_value;
        uint8_t hours_value;
        int seconds_flag;
        int minutes_flag;
        int hours_flag;
        uint8_t time_offset_length;
        uint32_t time_offset_value;
    } metadata_timecode;
    struct {
        uint8_t *buf;
        size_t buf_size;
    } unregistered;
} OBPMetadata;

/*******************
 * API structures. *
 *******************/

/*
 * OBPError contains a user-provided buffer and buffer size
 * where obuparse can write error messages to.
 */
typedef struct OBPError {
    char *error;
    size_t size;
} OBPError;

/******************
 * API functions. *
 ******************/

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

/*
 * obp_parse_sequence_header parses a sequence header OBU and fills out the fields in a
 * user-provided OBPSequenceHeader structure.
 *
 * Input:
 *     buf      - Input OBU buffer. This is expected to *NOT* contain the OBU header.
 *     buf_size - Size of the input OBU buffer.
 *     err      - An error buffer and buffer size to write any error messages into.
 *
 * Output:
 *     seq_header - A user provided struture that will be filled in with all the parsed data.
 *
 * Returns:
 *     0 on success, -1 on error.
 */
int obp_parse_sequence_header(uint8_t *buf, size_t buf_size, OBPSequenceHeader *seq_header, OBPError *err);

/*
 * obp_parse_metadata parses a metadata OBU and fills out the fields in a user-provided OBPMetadata
 * structure. This OBU's returned payload is *NOT* safe to use once the user-provided 'buf' has
 * been freed, since it may fill the structure with pointers to offsets that data.
 *
 * Input:
 *     buf      - Input OBU buffer. This is expected to *NOT* contain the OBU header.
 *     buf_size - Size of the input OBU buffer.
 *     err      - An error buffer and buffer size to write any error messages into.
 *
 * Output:
 *     metadata - A user provided struture that will be filled in with all the parsed data.
 *
 * Returns:
 *     0 on success, -1 on error.
 */
int obp_parse_metadata(uint8_t *buf, size_t buf_size, OBPMetadata *metadata, OBPError *err);

/*
 * obp_parse_tile_list parses a tile list OBU and fills out the fields in a user-provided OBPTileList
 * structure. This OBU's returned payload is *NOT* safe to use once the user-provided 'buf' has
 * been freed, since it may fill the structure with pointers to offsets that data.
 *
 * Input:
 *     buf      - Input OBU buffer. This is expected to *NOT* contain the OBU header.
 *     buf_size - Size of the input OBU buffer.
 *     err      - An error buffer and buffer size to write any error messages into.
 *
 * Output:
 *     tile_list - A user provided struture that will be filled in with all the parsed data.
 *
 * Returns:
 *     0 on success, -1 on error.
 */
int obp_parse_tile_list(uint8_t *buf, size_t buf_size, OBPTileList *tile_list, OBPError *err);

#endif
