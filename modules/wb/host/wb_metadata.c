#include "wb_metadata.h"
#include <string.h>

static uint32_t read32(const uint8_t *p, wb_byte_order order)
{
    if (order == WB_LITTLE_ENDIAN)
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
               ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    return (uint32_t)p[3] | ((uint32_t)p[2] << 8) |
           ((uint32_t)p[1] << 16) | ((uint32_t)p[0] << 24);
}

static void write32(uint8_t *p, uint32_t value, wb_byte_order order)
{
    unsigned int i;
    for (i = 0; i < 4u; ++i) {
        unsigned int shift = order == WB_LITTLE_ENDIAN ? i * 8u : (3u - i) * 8u;
        p[i] = (uint8_t)(value >> shift);
    }
}

static uint32_t gcd32(uint32_t a, uint32_t b)
{
    while (b != 0u) { uint32_t r = a % b; a = b; b = r; }
    return a;
}

static int valid_gain(wb_gain g)
{
    return g.numerator != 0u && g.denominator != 0u &&
           (uint64_t)g.numerator * 4u >= (uint64_t)g.denominator &&
           (uint64_t)g.numerator <= (uint64_t)g.denominator * 4u;
}

static int equal_gain(wb_gain a, wb_gain b)
{
    return (uint64_t)a.numerator * b.denominator ==
           (uint64_t)b.numerator * a.denominator;
}

static wb_status product32(const uint32_t factors[4], uint32_t *result)
{
    uint32_t value = 1u;
    unsigned int i;
    for (i = 0; i < 4u; ++i) {
        if (value > UINT32_MAX / factors[i]) return WB_EOVERFLOW;
        value *= factors[i];
    }
    *result = value;
    return WB_OK;
}

static wb_status ratio(wb_gain neutral, wb_gain green, wb_gain correction,
                       wb_gain green_correction, wb_gain *result)
{
    /* n_i / c_i / (n_g / c_g): four positive u32 factors per side.
     * Cancel every numerator/denominator pair before any multiplication.
     * Afterwards products are coprime, so overflow means exact RATIONAL cannot fit.
     */
    uint32_t numerator[4] = {neutral.numerator, correction.denominator,
                             green.denominator, green_correction.numerator};
    uint32_t denominator[4] = {neutral.denominator, correction.numerator,
                               green.numerator, green_correction.denominator};
    unsigned int i, j;
    wb_status status;
    for (i = 0; i < 4u; ++i)
        for (j = 0; j < 4u; ++j) {
            uint32_t divisor = gcd32(numerator[i], denominator[j]);
            numerator[i] /= divisor;
            denominator[j] /= divisor;
        }
    status = product32(numerator, &result->numerator);
    if (status != WB_OK) return status;
    return product32(denominator, &result->denominator);
}

wb_status wb_neutral_apply(uint8_t *destination, size_t destination_bytes,
                          const uint8_t *source, size_t source_bytes,
                          wb_byte_order order, const void *gains,
                          size_t gains_count)
{
    uint8_t original[WB_NEUTRAL_BYTES], output[WB_NEUTRAL_BYTES];
    wb_gain local_gains[WB_GAIN_COUNT], neutral[WB_GAIN_COUNT], result;
    unsigned int i;
    wb_status status;
    if (destination == NULL || source == NULL || gains == NULL) return WB_EARG;
    if (destination_bytes != WB_NEUTRAL_BYTES || source_bytes != WB_NEUTRAL_BYTES ||
        gains_count != WB_GAIN_COUNT) return WB_ESIZE;
    if (order != WB_LITTLE_ENDIAN && order != WB_BIG_ENDIAN) return WB_EORDER;
    /* Snapshot all inputs before any write, including possibly unaligned gains. */
    memcpy(original, source, sizeof original);
    memcpy(local_gains, gains, sizeof local_gains);
    for (i = 0; i < WB_GAIN_COUNT; ++i) {
        neutral[i].numerator = read32(original + i * 8u, order);
        neutral[i].denominator = read32(original + i * 8u + 4u, order);
        if (neutral[i].numerator == 0u || neutral[i].denominator == 0u)
            return WB_ENEUTRAL;
        if (!valid_gain(local_gains[i])) return WB_EGAIN;
    }
    if (equal_gain(local_gains[0], local_gains[1]) &&
        equal_gain(local_gains[1], local_gains[2])) {
        memcpy(destination, original, sizeof original);
        return WB_OK;
    }
    for (i = 0; i < WB_GAIN_COUNT; ++i) {
        status = ratio(neutral[i], neutral[1], local_gains[i], local_gains[1], &result);
        if (status != WB_OK) return status;
        write32(output + i * 8u, result.numerator, order);
        write32(output + i * 8u + 4u, result.denominator, order);
    }
    memcpy(destination, output, sizeof output);
    return WB_OK;
}

wb_status wb_config_validate(const wb_config *config)
{
    wb_config local;
    unsigned int i;
    if (config == NULL) return WB_EARG;
    memcpy(&local, config, sizeof local);
    if (local.enabled > 1u) return WB_ECONFIG;
    for (i = 0; i < WB_GAIN_COUNT; ++i)
        if (!valid_gain(local.gains[i])) return WB_EGAIN;
    return WB_OK;
}

wb_status wb_config_defaults(wb_config *config)
{
    wb_config local;
    unsigned int i;
    if (config == NULL) return WB_EARG;
    memset(&local, 0, sizeof local);
    for (i = 0; i < WB_GAIN_COUNT; ++i) {
        local.gains[i].numerator = 1u;
        local.gains[i].denominator = 1u;
    }
    memcpy(config, &local, sizeof local);
    return WB_OK;
}

static uint32_t crc32(const uint8_t *bytes, size_t count)
{
    uint32_t crc = UINT32_MAX;
    size_t i;
    unsigned int bit;
    for (i = 0; i < count; ++i) {
        crc ^= bytes[i];
        for (bit = 0; bit < 8u; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) != 0u ? UINT32_C(0xedb88320) : 0u);
    }
    return crc ^ UINT32_MAX;
}

wb_status wb_config_encode(uint8_t *destination, size_t bytes, const wb_config *config)
{
    uint8_t output[WB_CONFIG_BYTES];
    wb_config local;
    wb_status status;
    unsigned int i;
    if (destination == NULL || config == NULL) return WB_EARG;
    if (bytes != WB_CONFIG_BYTES) return WB_ESIZE;
    memcpy(&local, config, sizeof local);
    status = wb_config_validate(&local);
    if (status != WB_OK) return status;
    memset(output, 0, sizeof output);
    memcpy(output, "WBMC", 4u);
    output[4] = WB_CONFIG_VERSION;
    output[6] = WB_CONFIG_BYTES;
    write32(output + 8, local.enabled, WB_LITTLE_ENDIAN);
    write32(output + 12, WB_UNITS_SENSOR_MULTIPLIER, WB_LITTLE_ENDIAN);
    for (i = 0; i < WB_GAIN_COUNT; ++i) {
        write32(output + 16u + i * 8u, local.gains[i].numerator, WB_LITTLE_ENDIAN);
        write32(output + 20u + i * 8u, local.gains[i].denominator, WB_LITTLE_ENDIAN);
    }
    write32(output + 44, crc32(output, 44u), WB_LITTLE_ENDIAN);
    memcpy(destination, output, sizeof output);
    return WB_OK;
}

wb_status wb_config_decode(wb_config *destination, const uint8_t *source, size_t bytes)
{
    uint8_t input[WB_CONFIG_BYTES];
    wb_config local;
    wb_status status;
    unsigned int i;
    if (destination == NULL || source == NULL) return WB_EARG;
    if (bytes != WB_CONFIG_BYTES) return WB_ESIZE;
    memcpy(input, source, sizeof input);
    if (memcmp(input, "WBMC", 4u) != 0 || input[4] != WB_CONFIG_VERSION ||
        input[5] != 0u || input[6] != WB_CONFIG_BYTES || input[7] != 0u ||
        read32(input + 12, WB_LITTLE_ENDIAN) != WB_UNITS_SENSOR_MULTIPLIER ||
        read32(input + 40, WB_LITTLE_ENDIAN) != 0u ||
        read32(input + 44, WB_LITTLE_ENDIAN) != crc32(input, 44u)) return WB_ECONFIG;
    memset(&local, 0, sizeof local);
    local.enabled = read32(input + 8, WB_LITTLE_ENDIAN);
    for (i = 0; i < WB_GAIN_COUNT; ++i) {
        local.gains[i].numerator = read32(input + 16u + i * 8u, WB_LITTLE_ENDIAN);
        local.gains[i].denominator = read32(input + 20u + i * 8u, WB_LITTLE_ENDIAN);
    }
    status = wb_config_validate(&local);
    if (status != WB_OK) return status;
    memcpy(destination, &local, sizeof local);
    return WB_OK;
}

wb_status wb_runtime_init(wb_runtime *runtime)
{
    if (runtime == NULL) return WB_EARG;
    return wb_config_defaults(&runtime->config);
}

wb_status wb_runtime_set(wb_runtime *runtime, const wb_config *config)
{
    wb_config local;
    wb_status status;
    if (runtime == NULL || config == NULL) return WB_EARG;
    memcpy(&local, config, sizeof local);
    status = wb_config_validate(&local);
    if (status == WB_OK) memcpy(&runtime->config, &local, sizeof local);
    return status;
}

wb_status wb_runtime_load(wb_runtime *runtime, const uint8_t *source, size_t bytes)
{
    wb_config local;
    wb_status status;
    if (runtime == NULL) return WB_EARG;
    status = wb_config_decode(&local, source, bytes);
    if (status != WB_OK) (void)wb_config_defaults(&local);
    memcpy(&runtime->config, &local, sizeof local);
    return status;
}

wb_status wb_runtime_reset(wb_runtime *runtime)
{
    return wb_runtime_init(runtime);
}

wb_status wb_shot_init(wb_shot *shot)
{
    wb_shot local;
    if (shot == NULL) return WB_EARG;
    memset(&local, 0, sizeof local);
    (void)wb_config_defaults(&local.captured_config);
    local.state = WB_SHOT_EMPTY;
    memcpy(shot, &local, sizeof local);
    return WB_OK;
}

wb_status wb_shot_begin(wb_shot *shot, const wb_runtime *runtime, uint32_t mode_token)
{
    wb_shot local;
    wb_status status;
    if (shot == NULL || runtime == NULL) return WB_EARG;
    if (shot->state == WB_SHOT_PENDING || shot->state < WB_SHOT_EMPTY ||
        shot->state > WB_SHOT_FAILED) return WB_ESTATE;
    memset(&local, 0, sizeof local);
    memcpy(&local.captured_config, &runtime->config, sizeof local.captured_config);
    status = wb_config_validate(&local.captured_config);
    if (status != WB_OK) return status;
    local.mode_token = mode_token;
    local.state = WB_SHOT_PENDING;
    memcpy(shot, &local, sizeof local);
    return WB_OK;
}

wb_status wb_shot_cancel(wb_shot *shot)
{
    if (shot == NULL) return WB_EARG;
    if (shot->state != WB_SHOT_PENDING) return WB_ESTATE;
    shot->state = WB_SHOT_CANCELLED;
    return WB_OK;
}

wb_status wb_shot_finish(wb_shot *shot, uint8_t *destination, size_t destination_bytes,
                        const uint8_t *source, size_t source_bytes, wb_byte_order order)
{
    wb_config captured;
    wb_status status;
    if (shot == NULL) return WB_EARG;
    if (shot->state != WB_SHOT_PENDING) return WB_ESTATE;
    memcpy(&captured, &shot->captured_config, sizeof captured);
    status = wb_config_validate(&captured);
    if (status == WB_OK) {
        if (captured.enabled == 0u) (void)wb_config_defaults(&captured);
        status = wb_neutral_apply(destination, destination_bytes, source, source_bytes,
                                  order, captured.gains, WB_GAIN_COUNT);
    }
    shot->state = status == WB_OK ? WB_SHOT_COMPLETED : WB_SHOT_FAILED;
    return status;
}
