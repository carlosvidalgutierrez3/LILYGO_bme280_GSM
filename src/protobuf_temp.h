#include "nanopb/pb_common.h"
#include "nanopb/pb_encode.h"
#include "nanopb/pb.h"
#include "sf_msgs.pb.h"
#include "protobuf_temp_config.h"


Data3D measurments[MAX_MEASUREMENTS_] = Data3D_init_zero;
WrapperData msg = WrapperData_init_zero;

bool protobuf_converter(uint8_t (&msgBuffer)[128]) {
  //create the output stream for writing into our memory buffer.
  pb_ostream_t stream = pb_ostream_from_buffer(msgBuffer, sizeof(msgBuffer));
  //encode it to the binary format
  return pb_encode(&stream, WrapperData_fields, &msg);
}

bool generic3D_callback(pb_ostream_t *stream,
                        const pb_field_t *field,
                        void * const *arg) {
  for (int i = 0; i < MAX_MEASUREMENTS_; ++i) {
    if (!pb_encode_tag_for_field(stream, field)) return false;
    if (!pb_encode_submessage(stream,
                              Data3D_fields,
                              &measurments[i]))
      return false;
  }
  return true;
}

void resetMeasurments() {
  for (int i = 0; i < MAX_MEASUREMENTS_; i++) {
    measurments[i] = Data3D_init_zero;
  }
}
