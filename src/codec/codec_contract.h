/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file
 * @brief Fixed public codec parameter contract.
 *
 * This module is the authoritative description of the DSP parameters exposed
 * by the MVP BLE contract. It assigns stable block and parameter IDs, describes
 * the public byte count and access flags of each parameter, and provides lookup
 * by stable parameter ID.
 *
 * Contract membership implies read access. Parameter contents are opaque byte
 * arrays with no numerical representation or byte-order semantics in firmware.
 * Human-readable names are intentionally kept in the matching workstation
 * contract rather than transmitted by the firmware.
 *
 * IDs are dense, start at one, and match the order of
 * @ref codec_contract. The ordered four-byte definitions form the
 * public contract fingerprint. Any contract change must also update
 * @ref CODEC_PARAMETER_COUNT, @ref CODEC_PARAMETER_BYTE_COUNT,
 * @ref CODEC_CONTRACT_CRC32, and the workstation contract.
 *
 * This module owns immutable metadata only. It does not own runtime values,
 * persistence, BLE request handling, or codec access.
 */

#ifndef CODEC_CONTRACT_H
#define CODEC_CONTRACT_H

#include "adau1787.h"
#include <stddef.h>
#include <stdint.h>

/**
 * @brief CRC-32 fingerprint of the ordered public parameter contract.
 *
 * The fingerprint is calculated over the raw bytes of
 * @ref codec_contract and is used to detect firmware/workstation
 * contract mismatches.
 */
#define CODEC_CONTRACT_CRC32 0x7E8EB66CU

/** Number of entries in @ref codec_contract. */
#define CODEC_PARAMETER_COUNT 15U

/** Number of opaque bytes from number of DSP parameter words */
#define CODEC_BYTES_FROM_WORDS(words) ((uint8_t)(words) * ADAU1787_PARAM_RAM_WIDTH_BYTES)

/**
 * @brief Total number of parameter-value bytes in the fixed contract.
 *
 * This must equal the sum of every @ref codec_parameter.byte_count entry and
 * the combined size of the runtime parameter buffers.
 */
#define CODEC_PARAMETER_BYTE_COUNT                                                                                     \
  (6U * CODEC_BYTES_FROM_WORDS(1) + 8U * CODEC_BYTES_FROM_WORDS(34) + CODEC_BYTES_FROM_WORDS(45))

/** Size of the largest opaque parameter value in the fixed contract. */
#define CODEC_PARAMETER_MAX_BYTE_COUNT CODEC_BYTES_FROM_WORDS(45)

/**
 * @brief Public parameter access flags.
 *
 * Flags occupy one byte in @ref codec_parameter. They describe access only and
 * never assign a representation to the opaque parameter bytes.
 */
enum codec_contract_flag {
  /** The parameter accepts write requests; all parameters remain readable. */
  CODEC_CONTRACT_FLAG_WRITABLE = 1U << 0
};

/**
 * @brief Stable identifiers for DSP processing blocks.
 *
 * A block ID groups each parameter with its logical processing block. These
 * values are part of the firmware/workstation contract and must not be reused.
 */
enum dsp_block_id {
  /** ADC input selection block. */
  DSP_BLOCK_ID_ADC_SELECT = 1,
  /** Audio source selection block. */
  DSP_BLOCK_ID_SOURCE_SELECT = 2,
  /** Band 1 compressor block. */
  DSP_BLOCK_ID_BAND_1_COMPRESSOR = 3,
  /** Band 2 compressor block. */
  DSP_BLOCK_ID_BAND_2_COMPRESSOR = 4,
  /** Band 3 compressor block. */
  DSP_BLOCK_ID_BAND_3_COMPRESSOR = 5,
  /** Band 4 compressor block. */
  DSP_BLOCK_ID_BAND_4_COMPRESSOR = 6,
  /** Band 5 compressor block. */
  DSP_BLOCK_ID_BAND_5_COMPRESSOR = 7,
  /** Band 6 compressor block. */
  DSP_BLOCK_ID_BAND_6_COMPRESSOR = 8,
  /** Band 7 compressor block. */
  DSP_BLOCK_ID_BAND_7_COMPRESSOR = 9,
  /** Band 8 compressor block. */
  DSP_BLOCK_ID_BAND_8_COMPRESSOR = 10,
  /** First phase-compensation gain block. */
  DSP_BLOCK_ID_PHASE_COMP_GAIN_1 = 11,
  /** Second phase-compensation gain block. */
  DSP_BLOCK_ID_PHASE_COMP_GAIN_2 = 12,
  /** Third phase-compensation gain block. */
  DSP_BLOCK_ID_PHASE_COMP_GAIN_3 = 13,
  /** Output headroom gain block. */
  DSP_BLOCK_ID_OUTPUT_HEADROOM = 14,
  /** Soft-clip transfer-function block. */
  DSP_BLOCK_ID_SOFT_CLIP = 15,
};

/**
 * @brief Stable identifiers for parameters exposed over BLE.
 *
 * IDs are the only parameter identity carried by parameter read and write
 * requests. The workstation resolves each ID to a display name.
 */
typedef enum dsp_parameter_id {
  /** Reserved invalid or unset parameter identifier. */
  DSP_PARAMETER_ID_INVALID = 0,
  /** ADC selection parameter. */
  DSP_PARAMETER_ID_ADC_SELECT = 1,
  /** Source selection parameter. */
  DSP_PARAMETER_ID_SOURCE_SELECT = 2,
  /** Band 1 compressor lookup table. */
  DSP_PARAMETER_ID_BAND_1_COMPRESSOR_LUT = 3,
  /** Band 2 compressor lookup table. */
  DSP_PARAMETER_ID_BAND_2_COMPRESSOR_LUT = 4,
  /** Band 3 compressor lookup table. */
  DSP_PARAMETER_ID_BAND_3_COMPRESSOR_LUT = 5,
  /** Band 4 compressor lookup table. */
  DSP_PARAMETER_ID_BAND_4_COMPRESSOR_LUT = 6,
  /** Band 5 compressor lookup table. */
  DSP_PARAMETER_ID_BAND_5_COMPRESSOR_LUT = 7,
  /** Band 6 compressor lookup table. */
  DSP_PARAMETER_ID_BAND_6_COMPRESSOR_LUT = 8,
  /** Band 7 compressor lookup table. */
  DSP_PARAMETER_ID_BAND_7_COMPRESSOR_LUT = 9,
  /** Band 8 compressor lookup table. */
  DSP_PARAMETER_ID_BAND_8_COMPRESSOR_LUT = 10,
  /** First phase-compensation gain parameter. */
  DSP_PARAMETER_ID_PHASE_COMP_GAIN_1 = 11,
  /** Second phase-compensation gain parameter. */
  DSP_PARAMETER_ID_PHASE_COMP_GAIN_2 = 12,
  /** Third phase-compensation gain parameter. */
  DSP_PARAMETER_ID_PHASE_COMP_GAIN_3 = 13,
  /** Output headroom gain parameter. */
  DSP_PARAMETER_ID_OUTPUT_HEADROOM_GAIN = 14,
  /** Soft-clip lookup table. */
  DSP_PARAMETER_ID_SOFT_CLIP_LUT = 15,
} dsp_parameter_id;

/**
 * @brief Compact public definition of one DSP parameter.
 *
 * This four-byte representation is shared with the workstation contract and is
 * used to calculate the fixed contract fingerprint. It deliberately excludes
 * names, prescription constraints, and DSP addresses.
 */
struct codec_parameter {
  /** Stable @ref dsp_parameter_id value. */
  uint8_t id;

  /** Stable @ref dsp_block_id value for the owning processing block. */
  uint8_t block_id;

  /** Number of consecutive opaque bytes in the parameter. */
  uint8_t byte_count;

  /** Bitwise combination of @ref codec_contract_flag values. */
  uint8_t flags;
};

/**
 * @brief Ordered public parameter contract.
 *
 * Entries are ordered by ID, so a validated nonzero ID maps to array index
 * `id - 1`.
 *
 * The entry order and contents participate in
 * @ref CODEC_CONTRACT_CRC32. Any contract change must be mirrored in
 * `tiresias_workstation.domain.dsp_contract` and reflected in the contract
 * CRC.
 */
extern const struct codec_parameter codec_contract[CODEC_PARAMETER_COUNT];

/**
 * @brief Find a parameter definition by stable ID.
 *
 * @param id Stable parameter ID from the fixed contract.
 *
 * @return Pointer to the immutable contract entry, or NULL when @p id is not in
 * the contract. The returned pointer remains valid for the lifetime of the
 * firmware.
 */
const struct codec_parameter* codec_contract_find(uint8_t id);

/**
 * @brief Convert a validated stable parameter ID to its dense array index.
 *
 * @param id Nonzero parameter ID from the fixed contract.
 *
 * @return Zero-based array index corresponding to @p id.
 */
#define CODEC_PARAMETER_INDEX(id) ((size_t)(id) - 1U)

#endif /* CODEC_CONTRACT_H */
