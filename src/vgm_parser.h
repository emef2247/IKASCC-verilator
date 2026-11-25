#ifndef VGM_PARSER_H
#define VGM_PARSER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    VGM_ERROR_ARGUMENT = -1,
    VGM_ERROR_HEADER = -2,
    VGM_ERROR_TRUNCATED = -3,
    VGM_ERROR_UNSUPPORTED = -4,
    VGM_ERROR_IO = -5,
    VGM_ERROR_CHIP = -6
};

typedef enum {
    VGM_CHIP_UNKNOWN = 0,
    VGM_CHIP_K051649
} VGMChipId;

typedef struct VGMParserCallbacks {
    /* For K051649 (0xD2), the command carries three operand bytes:
     * port (register group; bit 7 is the dual-chip select), reg (the
     * register number within the group), and val (the data byte).
     * post_wait_samples is always zero; waits arrive via on_wait. */
    void (*on_reg_write)(void *user, VGMChipId chip, uint8_t port, uint8_t reg,
                         uint8_t val, uint32_t post_wait_samples);
    void (*on_wait)(void *user, uint32_t samples);
    void (*on_end)(void *user);
    /* Called immediately before rejecting an unknown or malformed command. */
    void (*on_unknown)(void *user, uint8_t opcode, uint32_t offset);
    void (*on_data_block)(void *user, uint8_t type_byte,
                          const uint8_t *data_ptr, uint32_t data_size);
} VGMParserCallbacks;

/* Plain or gzip-compressed VGM versions 1.10--1.71, one pass through the
 * stream. Only K051649 writes are delivered; writes to other chips are
 * skipped by their defined lengths so mixed-chip MSX files (PSG, OPLL,
 * SCC) render their SCC part. Loops are not followed. Unknown or
 * truncated commands fail. */
int vgm_parse_file(const char *path, const VGMParserCallbacks *cbs, void *user);
int vgm_parse_buffer(const uint8_t *buf, size_t size,
                     const VGMParserCallbacks *cbs, void *user);

/* Read the clock of a single standard K051649 from the 0xD0 header field.
 * Returns 0 when a nonzero clock was found, 1 when the field is absent or
 * zero (several exporters leave it zero; the caller applies its default),
 * and a negative VGM_ERROR_* code for a malformed header, a dual-chip
 * flag, or a reserved clock flag. */
int vgm_read_k051649_clock(const char *path, uint32_t *clock_hz);

#ifdef __cplusplus
}
#endif
#endif
