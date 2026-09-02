#include "nanopb_encoder.h"

namespace nanopb_encoder {

bool encode_bytes(pb_ostream_t *stream, const pb_field_t *field,
                  void *const *arg) {
    const auto *view = static_cast<const ByteView *>(*arg);
    return view != nullptr && pb_encode_tag_for_field(stream, field) &&
           pb_encode_string(stream, view->data, view->size);
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
