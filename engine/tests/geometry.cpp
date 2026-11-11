#include <catch2/catch.hpp>
#include <clipper2/clipper.h>

#include <algorithm>
#include <cstdint>

using namespace Clipper2Lib;

namespace {

Path64 rectangle(int64_t x, int64_t y, int64_t width, int64_t height)
{
    return {{x, y}, {x + width, y}, {x + width, y + height}, {x, y + height}};
}

Paths64 translated(Paths64 paths, int64_t dx, int64_t dy)
{
    for (auto &path : paths)
        for (auto &point : path) {
            point.x += dx;
            point.y += dy;
        }
    return paths;
}

} // namespace

TEST_CASE("Offset nozzle envelopes retain only their common printable region", "[geometry]")
{
    const Paths64 main = {rectangle(0, 0, 256000, 256000)};
    const Paths64 auxiliary = {rectangle(20500, 0, 235500, 256000)};
    const auto common = Intersect(main, auxiliary, FillRule::NonZero);

    REQUIRE(common.size() == 1);
    CHECK(Area(common) == Approx(235500.0 * 256000.0));
    for (const auto &point : common.front()) {
        CHECK(point.x >= 20500);
        CHECK(point.x <= 256000);
    }
}

TEST_CASE("Subtracting a hole preserves its empty interior", "[geometry]")
{
    const auto shell = Difference({rectangle(0, 0, 20000, 20000)},
                                 {rectangle(5000, 5000, 10000, 10000)},
                                 FillRule::NonZero);
    REQUIRE(shell.size() == 2);
    CHECK(Area(shell) == Approx(300000000.0));
    CHECK(Intersect(shell, {rectangle(6000, 6000, 8000, 8000)}, FillRule::NonZero).empty());
}

TEST_CASE("Fill rules distinguish nested contours with the same winding", "[geometry]")
{
    const Paths64 nested = {
        rectangle(0, 0, 20000, 20000),
        rectangle(5000, 5000, 10000, 10000)};

    CHECK(Area(Union(nested, FillRule::EvenOdd)) == Approx(300000000.0));
    CHECK(Area(Union(nested, FillRule::NonZero)) == Approx(400000000.0));
}

TEST_CASE("Inward wall offsets preserve the expected square cross section", "[geometry]")
{
    const auto wall = InflatePaths({rectangle(0, 0, 20000, 20000)},
                                   -450, JoinType::Miter, EndType::Polygon);
    REQUIRE(wall.size() == 1);
    CHECK(Area(wall) == Approx(19100.0 * 19100.0));
    CHECK(InflatePaths({rectangle(0, 0, 800, 20000)},
                       -450, JoinType::Miter, EndType::Polygon).empty());
}

TEST_CASE("Overlapping parts merge without counting the overlap twice", "[geometry]")
{
    const auto merged = Union(
        {rectangle(0, 0, 20000, 20000), rectangle(10000, 10000, 20000, 20000)},
        FillRule::NonZero);

    REQUIRE(merged.size() == 1);
    CHECK(Area(merged) == Approx(700000000.0));
}

TEST_CASE("Boolean results are invariant under a large coordinate translation", "[geometry]")
{
    const Paths64 subject = {rectangle(-10000, -10000, 20000, 20000)};
    const Paths64 clip = {rectangle(0, -5000, 20000, 10000)};
    const auto expected = Difference(subject, clip, FillRule::NonZero);
    const auto shifted = Difference(translated(subject, 1000000000, -1000000000),
                                    translated(clip, 1000000000, -1000000000),
                                    FillRule::NonZero);
    CHECK(Xor(expected, translated(shifted, -1000000000, 1000000000),
              FillRule::NonZero).empty());
}
