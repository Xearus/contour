#include <catch2/catch.hpp>
#include <GCodeChecker.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>

using BambuStudio::GCodeChecker;
using BambuStudio::GCodeCheckResult;

namespace {

std::string fixture()
{
    std::ifstream file(std::filesystem::path(CONTOUR_FIXTURE_DIR) / "line.gcode",
                       std::ios::binary);
    REQUIRE(file.is_open());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::string replace(std::string input, const std::string &before, const std::string &after)
{
    const auto position = input.find(before);
    REQUIRE(position != std::string::npos);
    return input.replace(position, before.size(), after);
}

class TemporaryGcode {
public:
    explicit TemporaryGcode(const std::string &input)
    {
        directory = std::filesystem::temp_directory_path() /
            ("contour-gcode-" + std::to_string(std::random_device{}()));
        REQUIRE(std::filesystem::create_directory(directory));
        path = directory / "input.gcode";
        std::ofstream file(path, std::ios::binary);
        REQUIRE(file.is_open());
        file << input;
        file.close();
        REQUIRE(file.good());
    }

    ~TemporaryGcode()
    {
        std::error_code error;
        std::filesystem::remove(path, error);
        std::filesystem::remove(directory, error);
    }

    std::filesystem::path path;

private:
    std::filesystem::path directory;
};

GCodeCheckResult check(const std::string &input)
{
    TemporaryGcode file(input);
    GCodeChecker checker;
    return checker.parse_file(file.path.string());
}

} // namespace

TEST_CASE("Checker accepts consistent relative and absolute extrusion", "[gcode]")
{
    CHECK(check(fixture()) == GCodeCheckResult::Success);
    auto absolute = replace(fixture(), "M83", "M82");
    absolute = replace(absolute, "X10 Y10 E0.33846", "X10 Y10 E0.67692");
    CHECK(check(absolute) == GCodeCheckResult::Success);
}

TEST_CASE("Checker accepts CRLF and a final line without a newline", "[gcode]")
{
    std::string crlf;
    for (const char character : fixture()) {
        if (character == '\n')
            crlf += '\r';
        crlf += character;
    }
    CHECK(check(crlf) == GCodeCheckResult::Success);

    auto no_newline = fixture();
    REQUIRE(no_newline.back() == '\n');
    no_newline.pop_back();
    CHECK(check(no_newline) == GCodeCheckResult::Success);
}

TEST_CASE("Checker rejects malformed movement axes and feed rates", "[gcode]")
{
    const auto movement = GENERATE(
        "G1 X10 X20 F1200",
        "G1 X10 F0",
        "G1 Xnan F1200",
        "G1 Xinf F1200",
        "G1 X F1200",
        "G1 I1 F1200");
    CAPTURE(movement);
    CHECK(check(fixture() + movement + "\n") == GCodeCheckResult::ParseFailed);
}

TEST_CASE("Checker rejects inconsistent extrusion width", "[gcode]")
{
    const auto excessive = replace(fixture(), "X10 Y0 E0.33846", "X10 Y0 E1");
    CHECK(check(excessive) != GCodeCheckResult::Success);
}

TEST_CASE("Checker checks arc radii", "[gcode]")
{
    CHECK(check(fixture() + "G2 X20 Y10 I5 J0 F1200\n") == GCodeCheckResult::Success);
    CHECK(check(fixture() + "G2 X20 Y15 I5 J0 F1200\n") != GCodeCheckResult::Success);
}

TEST_CASE("Checker rejects missing layer markers and invalid filament slots", "[gcode]")
{
    CHECK(check(replace(fixture(), "; CHANGE_LAYER\n", "")) == GCodeCheckResult::ParseFailed);
    CHECK(check(replace(fixture(), "T0\n", "T1\n")) == GCodeCheckResult::ParseFailed);
}

TEST_CASE("Checker rejects a single-nozzle temperature mismatch", "[gcode]")
{
    auto input = replace(fixture(), "M104 S210", "M104 S20");
    input = replace(input, "; Z_HEIGHT: 0.2", "; Z_HEIGHT: 0.4");
    input = replace(input, "; FEATURE: Inner wall", "; FEATURE: Outer wall");
    CHECK(check(input) == GCodeCheckResult::ParseFailed);
}

TEST_CASE("Known limit: the checker does not enforce the print envelope", "[known-limitations]")
{
    CHECK(check(fixture() + "G1 X1000 Y1000 F1200\n") == GCodeCheckResult::Success);
}

TEST_CASE("Known limit: dual-nozzle temperature mismatch can be accepted", "[known-limitations]")
{
    auto input = replace(fixture(), "T0\n", "; filament_map = 1,2\nT0\n");
    input = replace(input, "; filament_flow_ratio = 1\n", "; filament_flow_ratio = 1,1\n");
    input = replace(input, "; nozzle_temperature = 210\n", "; nozzle_temperature = 210,210\n");
    input = replace(input, "M104 S210", "M104 S20");
    input = replace(input, "; Z_HEIGHT: 0.2", "; Z_HEIGHT: 0.4");
    input = replace(input, "; FEATURE: Inner wall", "; FEATURE: Outer wall");
    CHECK(check(input) == GCodeCheckResult::Success);
}
