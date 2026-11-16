#include "contour/engine.h"

#include "libslic3r/Format/STL.hpp"
#include "libslic3r/GCode/GCodeProcessor.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/PrintConfig.hpp"

#include <cstdio>
#include <exception>
#include <filesystem>
#include <mutex>
#include <stdexcept>

namespace {

void write_error(char *error, size_t capacity, const char *message)
{
    if (error != nullptr && capacity != 0)
        std::snprintf(error, capacity, "%s", message);
}

}

extern "C" int contour_slice_stl(const char *input_path, const char *output_path,
                                 const char *config_path, char *error,
                                 size_t error_capacity)
{
    write_error(error, error_capacity, "");
    if (error == nullptr && error_capacity != 0)
        return CONTOUR_INVALID_ARGUMENT;
    if (input_path == nullptr || *input_path == '\0' ||
        output_path == nullptr || *output_path == '\0' ||
        (config_path != nullptr && *config_path == '\0')) {
        write_error(error, error_capacity, "Input, output and supplied config paths must be non-empty.");
        return CONTOUR_INVALID_ARGUMENT;
    }

    try {
        // Upstream has process-global state; do not promise concurrent slicing.
        static std::mutex slicing_mutex;
        const std::lock_guard<std::mutex> lock(slicing_mutex);
        if (std::filesystem::exists(output_path))
            throw std::runtime_error("Output already exists; choose a new path.");
        // Applying defaults through definitions initializes enum serialization maps.
        Slic3r::DynamicPrintConfig config;
        config.apply(Slic3r::FullPrintConfig::defaults());
        config.set_deserialize_strict({
            {"gcode_flavor", "marlin"},
            {"machine_start_gcode", "G90\\nM83"},
            {"machine_end_gcode", ""},
        });
        if (config_path != nullptr)
            config.load_from_ini(config_path, Slic3r::ForwardCompatibilitySubstitutionRule::Disable);
        const auto config_errors = config.validate(true);
        if (!config_errors.empty())
            throw std::runtime_error(config_errors.begin()->first + ": " +
                                     config_errors.begin()->second);
        Slic3r::Model model;
        if (!Slic3r::load_stl(input_path, &model) || model.objects.empty())
            throw std::runtime_error("Cannot load a non-empty STL model.");
        for (auto *object : model.objects) {
            object->add_instance();
            object->ensure_on_bed();
        }
        Slic3r::Print print;
        print.set_plate_origin(Slic3r::Vec3d::Zero());
        print.set_status_silent();
        print.apply(model, config);
        const auto validation_error = print.validate();
        if (!validation_error.string.empty())
            throw std::runtime_error(validation_error.string);
        print.process();
        Slic3r::GCodeProcessorResult result;
        print.export_gcode(output_path, &result);
        return CONTOUR_OK;
    } catch (const std::exception &exception) {
        write_error(error, error_capacity, exception.what());
        return CONTOUR_SLICE_FAILED;
    } catch (...) {
        write_error(error, error_capacity, "Unexpected native engine failure.");
        return CONTOUR_INTERNAL_ERROR;
    }
}
