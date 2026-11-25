#include "nanopb_encoder.h"

namespace nanopb_encoder {

bool encode_bytes(pb_ostream_t *stream, const pb_field_t *field,
                  void *const *arg) {
    const auto *view = static_cast<const ByteView *>(*arg);
    return view != nullptr && pb_encode_tag_for_field(stream, field) &&
           pb_encode_string(stream, view->data, view->size);
}

namespace {

bool encode_rle_runs(pb_ostream_t *stream, const ByteView &view) {
    for (size_t position = 0; position < view.size;) {
        const uint8_t value = view.data[position];
        size_t count = 1;
        while (++position < view.size && view.data[position] == value) ++count;

        if (!pb_encode_varint(stream, count) || !pb_write(stream, &value, 1))
            return false;
    }
    return true;
}

}  // namespace

bool encode_rle_bytes(pb_ostream_t *stream, const pb_field_t *field,
                      void *const *arg) {
    const auto *view = static_cast<const ByteView *>(*arg);
    if (view == nullptr || (view->data == nullptr && view->size != 0))
        return false;

    pb_ostream_t sizing = PB_OSTREAM_SIZING;
    return encode_rle_runs(&sizing, *view) &&
           pb_encode_tag_for_field(stream, field) &&
           pb_encode_varint(stream, sizing.bytes_written) &&
           encode_rle_runs(stream, *view);
}

bool encode_varints(pb_ostream_t *stream, const pb_field_t *field,
                    void *const *arg) {
    const auto *view = static_cast<const Uint32View *>(*arg);
    if (view == nullptr) return false;
    for (size_t index = 0; index < view->size; ++index) {
        if (!pb_encode_tag_for_field(stream, field) ||
            !pb_encode_varint(stream, view->data[index]))
            return false;
    }
    return true;
}

bool encode_fixed32s(pb_ostream_t *stream, const pb_field_t *field,
                     void *const *arg) {
    const auto *view = static_cast<const Uint32View *>(*arg);
    if (view == nullptr) return false;
    for (size_t index = 0; index < view->size; ++index) {
        if (!pb_encode_tag_for_field(stream, field) ||
            !pb_encode_fixed32(stream, &view->data[index]))
            return false;
    }
    return true;
}

}  // namespace nanopb_encoder
