#include "contour/engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition); \
        return EXIT_FAILURE; \
    } \
} while (0)

int main(int argc, char **argv)
{
    char error[256];
    struct {
        char before;
        char error[1];
        char after;
    } bounded = {'a', {'x'}, 'z'};
    CHECK(argc == 3);
    CHECK(contour_slice_stl(NULL, argv[2], NULL, error, sizeof(error)) ==
          CONTOUR_INVALID_ARGUMENT);
    CHECK(error[0] != '\0');
    CHECK(contour_slice_stl(argv[1], NULL, NULL, NULL, 0) == CONTOUR_INVALID_ARGUMENT);
    CHECK(contour_slice_stl("", argv[2], NULL, error, sizeof(error)) ==
          CONTOUR_INVALID_ARGUMENT);
    CHECK(contour_slice_stl(argv[1], argv[2], "", error, sizeof(error)) ==
          CONTOUR_INVALID_ARGUMENT);
    CHECK(contour_slice_stl(argv[1], argv[2], NULL, NULL, 1) == CONTOUR_INVALID_ARGUMENT);
    CHECK(contour_slice_stl(argv[1], argv[2], NULL, bounded.error, 1) ==
          CONTOUR_SLICE_FAILED);
    CHECK(bounded.before == 'a' && bounded.error[0] == '\0' && bounded.after == 'z');
    CHECK(contour_slice_stl(argv[1], argv[2], NULL, error, sizeof(error)) ==
          CONTOUR_SLICE_FAILED);
    CHECK(strstr(error, "STL") != NULL);
    CHECK(contour_slice_stl(argv[1], argv[2], NULL, NULL, 0) == CONTOUR_SLICE_FAILED);
    CHECK(contour_slice_stl(argv[1], argv[2], NULL, &bounded.before, 0) ==
          CONTOUR_SLICE_FAILED);
    CHECK(bounded.before == 'a');
    return EXIT_SUCCESS;
}
