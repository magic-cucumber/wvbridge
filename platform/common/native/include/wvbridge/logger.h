#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void notify_jvm_logger(const uint8_t level, const char* tag, const char* format, ...);

const char* logger_location_tag(const char* file, int line);

void logger_on_load(void);

void logger_on_unload(void);

#ifdef __cplusplus
}
#endif

#define LOGGER_V_TAGGABLE(tag, ...) notify_jvm_logger(0, (tag), __VA_ARGS__)
#define LOGGER_D_TAGGABLE(tag, ...) notify_jvm_logger(1, (tag), __VA_ARGS__)
#define LOGGER_I_TAGGABLE(tag, ...) notify_jvm_logger(2, (tag), __VA_ARGS__)
#define LOGGER_W_TAGGABLE(tag, ...) notify_jvm_logger(3, (tag), __VA_ARGS__)
#define LOGGER_E_TAGGABLE(tag, ...) notify_jvm_logger(4, (tag), __VA_ARGS__)
#define LOGGER_A_TAGGABLE(tag, ...) notify_jvm_logger(5, (tag), __VA_ARGS__)

#define LOGGER_V(...) \
    notify_jvm_logger(0, logger_location_tag(__FILE__, __LINE__), __VA_ARGS__)
#define LOGGER_D(...) \
    notify_jvm_logger(1, logger_location_tag(__FILE__, __LINE__), __VA_ARGS__)
#define LOGGER_I(...) \
    notify_jvm_logger(2, logger_location_tag(__FILE__, __LINE__), __VA_ARGS__)
#define LOGGER_W(...) \
    notify_jvm_logger(3, logger_location_tag(__FILE__, __LINE__), __VA_ARGS__)
#define LOGGER_E(...) \
    notify_jvm_logger(4, logger_location_tag(__FILE__, __LINE__), __VA_ARGS__)
#define LOGGER_A(...) \
    notify_jvm_logger(5, logger_location_tag(__FILE__, __LINE__), __VA_ARGS__)
