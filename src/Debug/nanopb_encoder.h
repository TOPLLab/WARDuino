#pragma once

#include "nanopb/pb_encode.h"

// Small, non-owning helpers for nanopb callback fields.  The pointed-to data
// must remain stable for nanopb's sizing and output passes.
namespace nanopb_encoder {

struct ByteView {
    const uint8_t *data = nullptr;
    size_t size = 0;
};

struct Uint32View {
    const uint32_t *data = nullptr;
    size_t size = 0;
};

bool encode_bytes(pb_ostream_t *stream, const pb_field_t *field,
                  void *const *arg);
bool encode_varints(pb_ostream_t *stream, const pb_field_t *field,
                    void *const *arg);
bool encode_fixed32s(pb_ostream_t *stream, const pb_field_t *field,
                     void *const *arg);

}  // namespace nanopb_encoder
