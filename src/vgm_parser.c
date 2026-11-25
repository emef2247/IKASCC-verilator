#include "vgm_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Load a whole file, inflating gzip data (magic 1F 8B) when present. */
static int load_file(const char *path, uint8_t **out, size_t *out_size) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return VGM_ERROR_IO;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return VGM_ERROR_IO; }
    long len = ftell(fp);
    if (len < 0) { fclose(fp); return VGM_ERROR_IO; }
    if (fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return VGM_ERROR_IO; }
    uint8_t *raw = (uint8_t *)malloc(len ? (size_t)len : 1);
    if (!raw) { fclose(fp); return VGM_ERROR_IO; }
    if (len && fread(raw, 1, (size_t)len, fp) != (size_t)len) { free(raw); fclose(fp); return VGM_ERROR_IO; }
    fclose(fp);
    if (len >= 2 && raw[0] == 0x1F && raw[1] == 0x8B) {
        z_stream zs; memset(&zs, 0, sizeof(zs));
        if (inflateInit2(&zs, 47) != Z_OK) { free(raw); return VGM_ERROR_IO; }
        size_t cap = (size_t)len * 6 + 65536, have = 0;
        uint8_t *buf = (uint8_t *)malloc(cap);
        if (!buf) { inflateEnd(&zs); free(raw); return VGM_ERROR_IO; }
        zs.next_in = raw; zs.avail_in = (uInt)len;
        int zrc;
        do {
            if (have == cap) {
                cap *= 2;
                uint8_t *nb = (uint8_t *)realloc(buf, cap);
                if (!nb) { free(buf); inflateEnd(&zs); free(raw); return VGM_ERROR_IO; }
                buf = nb;
            }
            zs.next_out = buf + have; zs.avail_out = (uInt)(cap - have);
            zrc = inflate(&zs, Z_NO_FLUSH);
            have = cap - zs.avail_out;
            if (zrc != Z_OK && zrc != Z_STREAM_END && zrc != Z_BUF_ERROR) {
                free(buf); inflateEnd(&zs); free(raw); return VGM_ERROR_IO;
            }
        } while (zrc != Z_STREAM_END);
        inflateEnd(&zs); free(raw);
        *out = buf; *out_size = have;
        return 0;
    }
    *out = raw; *out_size = (size_t)len;
    return 0;
}

/* Validate the header; report the data stream start/end and version. */
static int parse_header(const uint8_t *buf, size_t size,
                        size_t *data_start, size_t *data_end, uint32_t *version) {
    if (!buf || size < 0x40) return VGM_ERROR_HEADER;
    if (memcmp(buf, "Vgm ", 4) != 0) return VGM_ERROR_HEADER;
    uint32_t ver = rd32(buf + 8);
    if (ver < 0x110 || ver > 0x171) return VGM_ERROR_HEADER;
    uint32_t eof = rd32(buf + 4);
    if (eof == 0 || (uint64_t)eof + 4 > size) return VGM_ERROR_HEADER;
    size_t start;
    if (ver >= 0x150) {
        uint32_t off = rd32(buf + 0x34);
        if (off == 0) return VGM_ERROR_HEADER;
        start = 0x34 + (size_t)off;
    } else {
        start = 0x40;
    }
    if (start >= (size_t)eof + 4) return VGM_ERROR_HEADER;
    *data_start = start; *data_end = (size_t)eof + 4; *version = ver;
    return 0;
}

int vgm_read_k051649_clock(const char *path, uint32_t *clock_hz) {
    if (!path || !clock_hz) return VGM_ERROR_ARGUMENT;
    uint8_t *buf = NULL; size_t size = 0;
    int rc = load_file(path, &buf, &size);
    if (rc) return rc;
    size_t start, end; uint32_t ver;
    rc = parse_header(buf, size, &start, &end, &ver);
    if (rc) { free(buf); return rc; }
    if (start <= 0xD4) { free(buf); return 1; } /* field not present in header */
    uint32_t raw = rd32(buf + 0xD0);
    free(buf);
    if (raw & 0x80000000u) return VGM_ERROR_CHIP;   /* reserved flag */
    if (raw & 0x40000000u) return VGM_ERROR_CHIP;   /* dual chip: unsupported */
    uint32_t hz = raw & 0x3FFFFFFFu;
    if (!hz) return 1;
    *clock_hz = hz;
    return 0;
}

/* Operand counts for write commands that are skipped (other chips). */
static int skip_len(uint8_t cmd, int *len) {
    if (cmd == 0x4F || cmd == 0x50 || cmd == 0x30 || cmd == 0x31) { *len = 1; return 0; }
    if (cmd >= 0x32 && cmd <= 0x3F) { *len = 2; return 0; }
    if (cmd >= 0x51 && cmd <= 0x5F) { *len = 2; return 0; }
    if (cmd >= 0xA0 && cmd <= 0xBF) { *len = 2; return 0; }
    if (cmd >= 0xC0 && cmd <= 0xDF) { *len = 3; return 0; }
    if (cmd == 0xE0 || cmd == 0xE1) { *len = 4; return 0; }
    if (cmd == 0x90 || cmd == 0x91 || cmd == 0x95) { *len = 4; return 0; }
    if (cmd == 0x92) { *len = 5; return 0; }
    if (cmd == 0x93) { *len = 10; return 0; }
    if (cmd == 0x94) { *len = 1; return 0; }
    return -1;
}

int vgm_parse_buffer(const uint8_t *buf, size_t size,
                     const VGMParserCallbacks *cbs, void *user) {
    if (!buf || !cbs) return VGM_ERROR_ARGUMENT;
    size_t pos, end; uint32_t ver;
    int rc = parse_header(buf, size, &pos, &end, &ver);
    if (rc) return rc;
    uint32_t offset = 0;
    while (pos < end) {
        offset = (uint32_t)pos;
        uint8_t cmd = buf[pos++];
        if (cmd == 0x66) { if (cbs->on_end) cbs->on_end(user); return 0; }
        if (cmd == 0x61) {
            if (pos + 2 > end) goto truncated;
            uint32_t n = rd16(buf + pos); pos += 2;
            if (cbs->on_wait) cbs->on_wait(user, n);
            continue;
        }
        if (cmd == 0x62 || cmd == 0x63) { if (cbs->on_wait) cbs->on_wait(user, cmd == 0x62 ? 735 : 882); continue; }
        if (cmd >= 0x70 && cmd <= 0x7F) { if (cbs->on_wait) cbs->on_wait(user, (uint32_t)(cmd & 0x0F) + 1); continue; }
        if (cmd >= 0x80 && cmd <= 0x8F) { if (cbs->on_wait) cbs->on_wait(user, (uint32_t)(cmd & 0x0F)); continue; }
        if (cmd == 0x67) {
            if (pos + 6 > end || buf[pos] != 0x66) goto truncated;
            uint8_t type = buf[pos + 1];
            uint32_t n = rd32(buf + pos + 2);
            if (pos + 6 + n > end) goto truncated;
            if (cbs->on_data_block) cbs->on_data_block(user, type, buf + pos + 6, n);
            pos += 6 + n;
            continue;
        }
        if (cmd == 0x68) {
            if (pos + 11 > end) goto truncated;
            pos += 11;
            continue;
        }
        if (cmd == 0xD2) {
            if (pos + 3 > end) goto truncated;
            if (cbs->on_reg_write) cbs->on_reg_write(user, VGM_CHIP_K051649, buf[pos], buf[pos + 1], buf[pos + 2], 0);
            pos += 3;
            continue;
        }
        int len;
        if (skip_len(cmd, &len) == 0) {
            if (pos + (size_t)len > end) goto truncated;
            pos += (size_t)len;
            continue;
        }
        if (cbs->on_unknown) cbs->on_unknown(user, cmd, offset);
        return VGM_ERROR_UNSUPPORTED;
    }
    return VGM_ERROR_TRUNCATED;
truncated:
    if (cbs->on_unknown) cbs->on_unknown(user, buf[offset], offset);
    return VGM_ERROR_TRUNCATED;
}

int vgm_parse_file(const char *path, const VGMParserCallbacks *cbs, void *user) {
    if (!path) return VGM_ERROR_ARGUMENT;
    uint8_t *buf = NULL; size_t size = 0;
    int rc = load_file(path, &buf, &size);
    if (rc) return rc;
    rc = vgm_parse_buffer(buf, size, cbs, user);
    free(buf);
    return rc;
}
