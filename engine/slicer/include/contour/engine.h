#ifndef CONTOUR_ENGINE_H
#define CONTOUR_ENGINE_H

#include <stddef.h>

#if defined(_WIN32)
#ifdef CONTOUR_ENGINE_BUILD
#define CONTOUR_API __declspec(dllexport)
#else
#define CONTOUR_API __declspec(dllimport)
#endif
#else
#define CONTOUR_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum contour_status {
    CONTOUR_OK = 0,
    CONTOUR_INVALID_ARGUMENT = 1,
    CONTOUR_SLICE_FAILED = 2,
    CONTOUR_INTERNAL_ERROR = 3
};

/*
 * Synchronous, experimental STL-to-G-code operation; calls are serialized.
 * Paths are NUL-terminated native narrow strings. config_path may be NULL.
 * All supplied storage must remain valid and non-overlapping until return.
 * error may be NULL only if error_capacity is zero. Otherwise it receives a
 * NUL-terminated diagnostic (possibly truncated), or an empty string on success.
 * No allocation or C++ exception crosses this boundary. Existing outputs are
 * rejected, but output creation is not atomic and failures can leave partial
 * files. The generic output is not a printer-ready job.
 */
CONTOUR_API int contour_slice_stl(const char *input_path, const char *output_path,
                                 const char *config_path, char *error,
                                 size_t error_capacity);

#ifdef __cplusplus
}
#endif

#endif
