#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *buffer;
    size_t capacity;
    size_t pos;
} winder_link_payload_writer_t;

typedef struct {
    const uint8_t *buffer;
    size_t length;
    size_t pos;
} winder_link_payload_reader_t;

bool winder_link_payload_writer_init(
    winder_link_payload_writer_t *writer,
    uint8_t *buffer,
    size_t capacity
);

bool winder_link_payload_write_u8(
    winder_link_payload_writer_t *writer,
    uint8_t value
);

bool winder_link_payload_write_u16_le(
    winder_link_payload_writer_t *writer,
    uint16_t value
);

bool winder_link_payload_write_u32_le(
    winder_link_payload_writer_t *writer,
    uint32_t value
);

bool winder_link_payload_write_i32_le(
    winder_link_payload_writer_t *writer,
    int32_t value
);

size_t winder_link_payload_writer_len(
    const winder_link_payload_writer_t *writer
);

size_t winder_link_payload_writer_remaining(
    const winder_link_payload_writer_t *writer
);

bool winder_link_payload_reader_init(
    winder_link_payload_reader_t *reader,
    const uint8_t *buffer,
    size_t length
);

bool winder_link_payload_read_u8(
    winder_link_payload_reader_t *reader,
    uint8_t *out_value
);

bool winder_link_payload_read_u16_le(
    winder_link_payload_reader_t *reader,
    uint16_t *out_value
);

bool winder_link_payload_read_u32_le(
    winder_link_payload_reader_t *reader,
    uint32_t *out_value
);

bool winder_link_payload_read_i32_le(
    winder_link_payload_reader_t *reader,
    int32_t *out_value
);

size_t winder_link_payload_reader_remaining(
    const winder_link_payload_reader_t *reader
);

bool winder_link_payload_reader_done(
    const winder_link_payload_reader_t *reader
);
