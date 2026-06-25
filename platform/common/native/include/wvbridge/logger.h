#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void notify_jvm_logger(const char* level, const char* tag, const char* format, ...);

const char* logger_location_tag(const char* file, int line);

void logger_on_load(void);

void logger_on_unload(void);

#ifdef __cplusplus
}
#endif

#define LOGGER_V_TAGGABLE(tag, ...) notify_jvm_logger("VERBOSE", (tag), __VA_ARGS__)
#define LOGGER_D_TAGGABLE(tag, ...) notify_jvm_logger("DEBUG", (tag), __VA_ARGS__)
#define LOGGER_I_TAGGABLE(tag, ...) notify_jvm_logger("INFO", (tag), __VA_ARGS__)
#define LOGGER_W_TAGGABLE(tag, ...) notify_jvm_logger("WARN", (tag), __VA_ARGS__)
#define LOGGER_E_TAGGABLE(tag, ...) notify_jvm_logger("ERROR", (tag), __VA_ARGS__)
#define LOGGER_A_TAGGABLE(tag, ...) notify_jvm_logger("ASSERT", (tag), __VA_ARGS__)

#define LOGGER_V(...) \
    notify_jvm_logger("VERBOSE", logger_location_tag(__FILE__, __LINE__), __VA_ARGS__)
#define LOGGER_D(...) \
    notify_jvm_logger("DEBUG", logger_location_tag(__FILE__, __LINE__), __VA_ARGS__)
#define LOGGER_I(...) \
    notify_jvm_logger("INFO", logger_location_tag(__FILE__, __LINE__), __VA_ARGS__)
#define LOGGER_W(...) \
    notify_jvm_logger("WARN", logger_location_tag(__FILE__, __LINE__), __VA_ARGS__)
#define LOGGER_E(...) \
    notify_jvm_logger("ERROR", logger_location_tag(__FILE__, __LINE__), __VA_ARGS__)
#define LOGGER_A(...) \
    notify_jvm_logger("ASSERT", logger_location_tag(__FILE__, __LINE__), __VA_ARGS__)
