#include "libslic3r/Format/STL.hpp"
#include "libslic3r/GCode/GCodeProcessor.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/PrintConfig.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>

int main(int argc, char **argv)
{
    if (argc != 3 && argc != 4) {
        std::cerr << "Usage: contour-slice INPUT.stl OUTPUT.gcode [CONFIG.ini]\n"
                     "Experimental generic profile; not a printer-ready job.\n";
        return 2;
    }
    try {
        if (std::filesystem::exists(argv[2]))
            throw std::runtime_error("Output already exists; choose a new path.");
        // Applying defaults through definitions initializes enum serialization maps.
        Slic3r::DynamicPrintConfig config;
        config.apply(Slic3r::FullPrintConfig::defaults());
        config.set_deserialize_strict({
            {"gcode_flavor", "marlin"},
            {"machine_start_gcode", "G90\\nM83"},
            {"machine_end_gcode", ""},
        });
        if (argc == 4)
            config.load_from_ini(argv[3], Slic3r::ForwardCompatibilitySubstitutionRule::Disable);
        const auto config_errors = config.validate(true);
        if (!config_errors.empty())
            throw std::runtime_error(config_errors.begin()->first + ": " +
                                     config_errors.begin()->second);
        Slic3r::Model model;
        if (!Slic3r::load_stl(argv[1], &model) || model.objects.empty())
            throw std::runtime_error("Cannot load a non-empty STL model.");
        for (auto *object : model.objects) {
            object->add_instance();
            object->ensure_on_bed();
        }
        Slic3r::Print print;
        print.set_status_silent();
        print.apply(model, config);
        const auto error = print.validate();
        if (!error.string.empty())
            throw std::runtime_error(error.string);
        print.process();
        Slic3r::GCodeProcessorResult result;
        print.export_gcode(argv[2], &result);
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Slicing failed: " << error.what() << '\n';
        return 1;
    }
}
