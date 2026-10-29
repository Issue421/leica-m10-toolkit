#ifndef WB_TARGET_H
#define WB_TARGET_H
#include <stddef.h>
#include <stdint.h>
#define WB_NEUTRAL_BYTES 24u
#define WB_CONFIG_WIRE_BYTES 88u
#define WB_CONFIG_VERSION 1u
#define WB_MODEL_M10P UINT32_C(0x05263551)
#define WB_MODEL_M10 UINT32_C(0x04263610)
typedef enum { WB_LITTLE_ENDIAN=0, WB_BIG_ENDIAN=1 } wb_byte_order;
typedef enum {
 WB_OK=0, WB_EARG=1, WB_ESIZE=2, WB_EORDER=3, WB_ENEUTRAL=4,
 WB_EGAIN=5, WB_EOVERFLOW=6, WB_ECONFIG=7, WB_EPROFILE=8,
 WB_ESTALE=9, WB_ESIGNED=10, WB_EBUILDER=11, WB_EALIAS=12
} wb_status;
typedef struct { uint32_t numerator,denominator; } wb_gain;
typedef struct {
 uint32_t model; uint8_t img_sha256[32]; char version[16];
 uint32_t wb_offset[3],snapshot_bytes;
} wb_profile;
/* This constant describes the selected build. It does not attest a camera. */
extern const wb_profile wb_selected_profile;
typedef struct {
 uint32_t version,model; uint8_t img_sha256[32];
 uint32_t enabled,epoch,generation; wb_gain gains[3];
} wb_config;
typedef struct { uint32_t model,mode_token,epoch,generation; } wb_provenance;
typedef struct { wb_config config; wb_provenance provenance; } wb_shot;
_Static_assert(sizeof(wb_gain)==8 && _Alignof(wb_gain)==4,"gain ABI");
_Static_assert(sizeof(wb_profile)==68 && _Alignof(wb_profile)==4,"profile ABI");
_Static_assert(sizeof(wb_config)==76 && _Alignof(wb_config)==4,"config ABI");
_Static_assert(offsetof(wb_config,gains)==52,"gain offset ABI");
_Static_assert(sizeof(wb_provenance)==16 && sizeof(wb_shot)==92,"shot ABI");
_Static_assert(_Alignof(wb_shot)==4 && offsetof(wb_shot,provenance)==76,"shot alignment ABI");
_Static_assert(sizeof(wb_status)==4 && sizeof(wb_byte_order)==4,"enum ABI");
#ifdef WB_TARGET_ABI
_Static_assert(sizeof(void *)==4 && sizeof(size_t)==4 && sizeof(uintptr_t)==4,"AAPCS32 sizes");
#endif
/* Exact R/G/B rational correction. gains is an alignment-neutral native byte
 * span of three wb_gain records; all overlaps are supported. Every error leaves
 * all bytes unchanged. On success source bytes shared with destination change.
 * Caller admits the 24B TIFF C628 span and serializes access to all objects. */
wb_status wb_neutral_apply(uint8_t *destination,size_t destination_bytes,
 const uint8_t *original,size_t original_bytes,wb_byte_order order,
 const void *gains,size_t gains_count);
wb_status wb_config_validate(const wb_config *config);
wb_status wb_config_defaults(wb_config *destination,uint32_t epoch,uint32_t generation);
/* Wire format is explicit LE/CRC32; never persist the native struct. Codec
 * overlap is supported; errors preserve destination. No persistence I/O. */
wb_status wb_config_encode(uint8_t *destination,size_t bytes,const wb_config *config);
wb_status wb_config_decode(wb_config *destination,const uint8_t *source,size_t bytes);
/* Copies the immutable config and provenance, retaining no external pointers.
 * Output must be disjoint from inputs. All errors preserve every input/output. */
wb_status wb_shot_prepare(wb_shot *destination,const wb_config *config,
 const wb_provenance *provenance);
/* Pure, repeatable transform of an ORIGINAL span, not a lifecycle/cache/hook.
 * expected must be independently supplied by the serialized owner for the
 * current shot. Rejects stale/mismatched model/mode/epoch/generation. Output
 * cannot overlap shot/expected; source/output overlap remains supported. */
wb_status wb_shot_apply(uint8_t *destination,size_t destination_bytes,
 const uint8_t *original,size_t original_bytes,wb_byte_order order,
 const wb_shot *shot,const wb_provenance *expected);
/* Bounded, read-only snapshot adapter. snapshot_bytes is the exact selected
 * pool-object extent (P0x720/M0x718), not a device address. Signed16 LE channels
 * must be positive before unsigned use; original24 must contain the builder's
 * exact 256/channel pairs in order. Output is disjoint from every input.
 * Caller owns accessible memory; no hook, function pointer or target I/O. */
wb_status wb_snapshot_apply(uint8_t *destination,size_t destination_bytes,
 const uint8_t *original,size_t original_bytes,wb_byte_order order,
 const uint8_t *snapshot,size_t snapshot_bytes,
 const wb_shot *shot,const wb_provenance *expected);
#endif
