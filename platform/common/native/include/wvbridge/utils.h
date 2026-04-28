#pragma once

#include <jni.h>

#include "wvbridge/java_caller.h"

#ifdef __cplusplus
extern "C" {
#endif

java_caller_status java_caller_pack_boolean(jboolean value, jvalue* out_value);
java_caller_status java_caller_pack_byte(jbyte value, jvalue* out_value);
java_caller_status java_caller_pack_char(jchar value, jvalue* out_value);
java_caller_status java_caller_pack_short(jshort value, jvalue* out_value);
java_caller_status java_caller_pack_int(jint value, jvalue* out_value);
java_caller_status java_caller_pack_long(jlong value, jvalue* out_value);
java_caller_status java_caller_pack_float(jfloat value, jvalue* out_value);
java_caller_status java_caller_pack_double(jdouble value, jvalue* out_value);

java_caller_status java_caller_depack_boolean(jvalue value, jboolean* out_value);
java_caller_status java_caller_depack_byte(jvalue value, jbyte* out_value);
java_caller_status java_caller_depack_char(jvalue value, jchar* out_value);
java_caller_status java_caller_depack_short(jvalue value, jshort* out_value);
java_caller_status java_caller_depack_int(jvalue value, jint* out_value);
java_caller_status java_caller_depack_long(jvalue value, jlong* out_value);
java_caller_status java_caller_depack_float(jvalue value, jfloat* out_value);
java_caller_status java_caller_depack_double(jvalue value, jdouble* out_value);

#ifdef __cplusplus
}
#endif
