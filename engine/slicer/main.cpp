#include "contour/engine.h"

#include <iostream>

int main(int argc, char **argv)
{
    if (argc != 3 && argc != 4) {
        std::cerr << "Usage: contour-slice INPUT.stl OUTPUT.gcode [CONFIG.ini]\n"
                     "Experimental generic profile; not a printer-ready job.\n";
        return 2;
    }
    char error[4096];
    if (contour_slice_stl(argv[1], argv[2], argc == 4 ? argv[3] : nullptr,
                          error, sizeof(error)) != CONTOUR_OK) {
        std::cerr << "Slicing failed: " << error << '\n';
        return 1;
    }
    return 0;
}
