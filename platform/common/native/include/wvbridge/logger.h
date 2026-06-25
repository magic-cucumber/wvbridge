#pragma once

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

void notify_jvm_logger(jlong pointer, const char* level, const char* tag, const char* format, ...);

const char* logger_location_tag(const char* file, int line);

void logger_on_load(void);

void logger_on_unload(void);

#ifdef __cplusplus
}
#endif

#define LOGGER_V_TAGGABLE(pointer, tag, ...) notify_jvm_logger((pointer), "VERBOSE", (tag), __VA_ARGS__)
#define LOGGER_D_TAGGABLE(pointer, tag, ...) notify_jvm_logger((pointer), "DEBUG", (tag), __VA_ARGS__)
#define LOGGER_I_TAGGABLE(pointer, tag, ...) notify_jvm_logger((pointer), "INFO", (tag), __VA_ARGS__)
#define LOGGER_W_TAGGABLE(pointer, tag, ...) notify_jvm_logger((pointer), "WARN", (tag), __VA_ARGS__)
#define LOGGER_E_TAGGABLE(pointer, tag, ...) notify_jvm_logger((pointer), "ERROR", (tag), __VA_ARGS__)
#define LOGGER_A_TAGGABLE(pointer, tag, ...) notify_jvm_logger((pointer), "ASSERT", (tag), __VA_ARGS__)

#define LOGGER_V(pointer, ...) \
    notify_jvm_logger((pointer), "VERBOSE", logger_location_tag(__FILE__, __LINE__), __VA_ARGS__)
#define LOGGER_D(pointer, ...) \
    notify_jvm_logger((pointer), "DEBUG", logger_location_tag(__FILE__, __LINE__), __VA_ARGS__)
#define LOGGER_I(pointer, ...) \
    notify_jvm_logger((pointer), "INFO", logger_location_tag(__FILE__, __LINE__), __VA_ARGS__)
#define LOGGER_W(pointer, ...) \
    notify_jvm_logger((pointer), "WARN", logger_location_tag(__FILE__, __LINE__), __VA_ARGS__)
#define LOGGER_E(pointer, ...) \
    notify_jvm_logger((pointer), "ERROR", logger_location_tag(__FILE__, __LINE__), __VA_ARGS__)
#define LOGGER_A(pointer, ...) \
    notify_jvm_logger((pointer), "ASSERT", logger_location_tag(__FILE__, __LINE__), __VA_ARGS__)
