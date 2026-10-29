#ifndef WB_METADATA_H
#define WB_METADATA_H

/* Portable C99 host/reference core. No allocation, I/O or device addresses. */
#include <stddef.h>
#include <stdint.h>

#define WB_NEUTRAL_BYTES 24u
#define WB_GAIN_COUNT 3u
#define WB_CONFIG_BYTES 48u
#define WB_CONFIG_VERSION 1u
#define WB_UNITS_SENSOR_MULTIPLIER 1u

typedef enum { WB_LITTLE_ENDIAN = 0, WB_BIG_ENDIAN = 1 } wb_byte_order;
typedef enum {
    WB_OK = 0, WB_EARG = 1, WB_ESIZE = 2, WB_EORDER = 3,
    WB_ENEUTRAL = 4, WB_EGAIN = 5, WB_EOVERFLOW = 6,
    WB_ECONFIG = 7, WB_ESTATE = 8
} wb_status;
typedef struct { uint32_t numerator, denominator; } wb_gain;
typedef struct { uint32_t enabled; wb_gain gains[WB_GAIN_COUNT]; } wb_config;
typedef struct { wb_config config; } wb_runtime;
typedef enum {
    WB_SHOT_EMPTY = 0, WB_SHOT_PENDING = 1, WB_SHOT_COMPLETED = 2,
    WB_SHOT_CANCELLED = 3, WB_SHOT_FAILED = 4
} wb_shot_state;
typedef struct {
    /* Read-only to consumers after begin; API never changes these while pending. */
    wb_config captured_config;
    uint32_t mode_token;
    wb_shot_state state;
} wb_shot;

/* Exactly 24 source and destination bytes; gains_count must be 3.
 * gains points to the native object representation of 3 wb_gain records;
 * unaligned gain bytes are accepted (no typed pointer conversion is required).
 * All errors leave destination unchanged. All overlapping source/destination/
 * gains spans are supported. Equal rational gains preserve source bytes.
 * Caller owns valid object extents and serialization; no concurrency primitives.
 */
wb_status wb_neutral_apply(uint8_t *destination, size_t destination_bytes,
                          const uint8_t *source, size_t source_bytes,
                          wb_byte_order order, const void *gains,
                          size_t gains_count);
wb_status wb_config_validate(const wb_config *config);
wb_status wb_config_defaults(wb_config *config);
/* Codec is canonical little endian, CRC32 protected, independent of struct ABI.
 * Errors leave destination unchanged. Overlapping input/output is supported.
 */
wb_status wb_config_encode(uint8_t *destination, size_t bytes,
                           const wb_config *config);
wb_status wb_config_decode(wb_config *destination, const uint8_t *source,
                           size_t bytes);
wb_status wb_runtime_init(wb_runtime *runtime);
wb_status wb_runtime_set(wb_runtime *runtime, const wb_config *config);
/* Invalid persisted input resets FUTURE shots to disabled factory defaults.
 * Returns the decode error so callers can report it. Pending shots are untouched.
 */
wb_status wb_runtime_load(wb_runtime *runtime, const uint8_t *source, size_t bytes);
wb_status wb_runtime_reset(wb_runtime *runtime);
wb_status wb_shot_init(wb_shot *shot);
/* Init required; an existing pending shot cannot be overwritten. mode_token is
 * opaque original-mode provenance, never interpreted or changed by this core.
 */
wb_status wb_shot_begin(wb_shot *shot, const wb_runtime *runtime, uint32_t mode_token);
wb_status wb_shot_cancel(wb_shot *shot);
/* Exactly one successful finish/cancel per pending shot. Failure is terminal;
 * destination remains untouched and caller retains original metadata as fallback.
 * Disabled config uses equal identity gains and still validates original neutral.
 */
wb_status wb_shot_finish(wb_shot *shot, uint8_t *destination, size_t destination_bytes,
                        const uint8_t *source, size_t source_bytes, wb_byte_order order);

#endif
