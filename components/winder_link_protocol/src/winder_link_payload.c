#include "winder_link_payload.h"

static bool writer_has_space(const winder_link_payload_writer_t *writer, size_t bytes)
{
    if (writer == NULL || writer->buffer == NULL) {
        return false;
    }

    if (writer->pos > writer->capacity) {
        return false;
    }

    return (writer->capacity - writer->pos) >= bytes;
}

static bool reader_has_bytes(const winder_link_payload_reader_t *reader, size_t bytes)
{
    if (reader == NULL) {
        return false;
    }

    if (reader->buffer == NULL && reader->length > 0U) {
        return false;
    }

    if (reader->pos > reader->length) {
        return false;
    }

    return (reader->length - reader->pos) >= bytes;
}

bool winder_link_payload_writer_init(
    winder_link_payload_writer_t *writer,
    uint8_t *buffer,
    size_t capacity)
{
    if (writer == NULL) {
        return false;
    }

    if (buffer == NULL && capacity > 0U) {
        return false;
    }

    writer->buffer = buffer;
    writer->capacity = capacity;
    writer->pos = 0U;
    return true;
}

bool winder_link_payload_write_u8(
    winder_link_payload_writer_t *writer,
    uint8_t value)
{
    if (!writer_has_space(writer, 1U)) {
        return false;
    }

    writer->buffer[writer->pos] = value;
    writer->pos += 1U;
    return true;
}

bool winder_link_payload_write_u16_le(
    winder_link_payload_writer_t *writer,
    uint16_t value)
{
    if (!writer_has_space(writer, 2U)) {
        return false;
    }

    writer->buffer[writer->pos] = (uint8_t)(value & 0xFFU);
    writer->buffer[writer->pos + 1U] = (uint8_t)(value >> 8);
    writer->pos += 2U;
    return true;
}

bool winder_link_payload_write_u32_le(
    winder_link_payload_writer_t *writer,
    uint32_t value)
{
    if (!writer_has_space(writer, 4U)) {
        return false;
    }

    writer->buffer[writer->pos] = (uint8_t)(value & 0xFFU);
    writer->buffer[writer->pos + 1U] = (uint8_t)((value >> 8) & 0xFFU);
    writer->buffer[writer->pos + 2U] = (uint8_t)((value >> 16) & 0xFFU);
    writer->buffer[writer->pos + 3U] = (uint8_t)(value >> 24);
    writer->pos += 4U;
    return true;
}

bool winder_link_payload_write_i32_le(
    winder_link_payload_writer_t *writer,
    int32_t value)
{
    return winder_link_payload_write_u32_le(writer, (uint32_t)value);
}

size_t winder_link_payload_writer_len(
    const winder_link_payload_writer_t *writer)
{
    if (writer == NULL) {
        return 0U;
    }

    return writer->pos;
}

size_t winder_link_payload_writer_remaining(
    const winder_link_payload_writer_t *writer)
{
    if (writer == NULL || writer->pos > writer->capacity) {
        return 0U;
    }

    return writer->capacity - writer->pos;
}

bool winder_link_payload_reader_init(
    winder_link_payload_reader_t *reader,
    const uint8_t *buffer,
    size_t length)
{
    if (reader == NULL) {
        return false;
    }

    if (buffer == NULL && length > 0U) {
        return false;
    }

    reader->buffer = buffer;
    reader->length = length;
    reader->pos = 0U;
    return true;
}

bool winder_link_payload_read_u8(
    winder_link_payload_reader_t *reader,
    uint8_t *out_value)
{
    if (out_value == NULL || !reader_has_bytes(reader, 1U)) {
        return false;
    }

    *out_value = reader->buffer[reader->pos];
    reader->pos += 1U;
    return true;
}

bool winder_link_payload_read_u16_le(
    winder_link_payload_reader_t *reader,
    uint16_t *out_value)
{
    if (out_value == NULL || !reader_has_bytes(reader, 2U)) {
        return false;
    }

    *out_value = (uint16_t)reader->buffer[reader->pos] |
                 ((uint16_t)reader->buffer[reader->pos + 1U] << 8);
    reader->pos += 2U;
    return true;
}

bool winder_link_payload_read_u32_le(
    winder_link_payload_reader_t *reader,
    uint32_t *out_value)
{
    if (out_value == NULL || !reader_has_bytes(reader, 4U)) {
        return false;
    }

    *out_value = (uint32_t)reader->buffer[reader->pos] |
                 ((uint32_t)reader->buffer[reader->pos + 1U] << 8) |
                 ((uint32_t)reader->buffer[reader->pos + 2U] << 16) |
                 ((uint32_t)reader->buffer[reader->pos + 3U] << 24);
    reader->pos += 4U;
    return true;
}

bool winder_link_payload_read_i32_le(
    winder_link_payload_reader_t *reader,
    int32_t *out_value)
{
    uint32_t value = 0U;
    if (out_value == NULL) {
        return false;
    }

    if (!winder_link_payload_read_u32_le(reader, &value)) {
        return false;
    }

    *out_value = (int32_t)value;
    return true;
}

size_t winder_link_payload_reader_remaining(
    const winder_link_payload_reader_t *reader)
{
    if (reader == NULL || reader->pos > reader->length) {
        return 0U;
    }

    return reader->length - reader->pos;
}

bool winder_link_payload_reader_done(
    const winder_link_payload_reader_t *reader)
{
    return reader != NULL && reader->pos == reader->length;
}
