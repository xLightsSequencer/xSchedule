// Blend modes used when a playlist step layers one channel buffer over another.

#include <catch2/catch_test_macros.hpp>

#include "Blend.h"

#include <vector>

namespace {
using Buf = std::vector<uint8_t>;

Buf Apply(Buf base, Buf top, APPLYMETHOD m, size_t offset = 0) {
    Blend(base.data(), base.size(), top.data(), top.size(), m, offset);
    return base;
}
} // namespace

TEST_CASE("Blend mode names round trip", "[blend]") {
    for (APPLYMETHOD m : { METHOD_OVERWRITE, METHOD_OVERWRITEIFZERO, METHOD_OVERWRITESKIPBLACK, METHOD_MASK,
                           METHOD_UNMASK, METHOD_AVERAGE, METHOD_MAX, METHOD_OVERWRITEIFBLACK, METHOD_MASKPIXEL,
                           METHOD_UNMASKPIXEL, METHOD_MIN, METHOD_BRIGHTNESS }) {
        CAPTURE(DecodeBlendMode(m));
        CHECK(EncodeBlendMode(DecodeBlendMode(m)) == m);
    }
    CHECK(EncodeBlendMode("MAXIMUM") == METHOD_MAX);         // case insensitive
    CHECK(EncodeBlendMode("no such mode") == METHOD_OVERWRITE);
}

TEST_CASE("Per channel blend modes", "[blend]") {
    const Buf base{ 0, 10, 200, 0, 100, 255 };
    const Buf top{ 50, 0, 100, 0, 200, 1 };

    CHECK(Apply(base, top, METHOD_OVERWRITE) == top);
    CHECK(Apply(base, top, METHOD_OVERWRITEIFZERO) == Buf{ 50, 10, 200, 0, 100, 255 });
    CHECK(Apply(base, top, METHOD_MASK) == Buf{ 0, 10, 0, 0, 0, 0 });      // blank where top is non-zero
    CHECK(Apply(base, top, METHOD_UNMASK) == Buf{ 0, 0, 200, 0, 100, 255 });  // blank where top is zero
    CHECK(Apply(base, top, METHOD_AVERAGE) == Buf{ 25, 5, 150, 0, 150, 128 });
    CHECK(Apply(base, top, METHOD_MAX) == Buf{ 50, 10, 200, 0, 200, 255 });
    CHECK(Apply(base, top, METHOD_MIN) == Buf{ 0, 0, 100, 0, 100, 1 });
}

TEST_CASE("Per pixel blend modes", "[blend]") {
    //               black       red          white
    const Buf base{ 0, 0, 0,    255, 0, 0,   255, 255, 255 };
    const Buf top{ 1, 2, 3,     0, 0, 0,     0, 0, 9 };

    CHECK(Apply(base, top, METHOD_OVERWRITEIFBLACK) == Buf{ 1, 2, 3,   255, 0, 0,   255, 255, 255 });
    CHECK(Apply(base, top, METHOD_OVERWRITESKIPBLACK) == Buf{ 1, 2, 3,   255, 0, 0,   0, 0, 9 });
    CHECK(Apply(base, top, METHOD_MASKPIXEL) == Buf{ 0, 0, 0,   255, 0, 0,   0, 0, 0 });
    CHECK(Apply(base, top, METHOD_UNMASKPIXEL) == Buf{ 0, 0, 0,   0, 0, 0,   255, 255, 255 });
}

TEST_CASE("Brightness scales by the top value", "[blend]") {
    const Buf base{ 200, 200, 200,   100, 100, 100 };
    const Buf top{ 255, 0, 51,       255, 255, 128 };
    CHECK(Apply(base, top, METHOD_BRIGHTNESS) == Buf{ 200, 0, 40,   100, 100, 50 });
}

TEST_CASE("Blend at an offset and clipped to the buffer", "[blend]") {
    CHECK(Apply(Buf(5, 1), Buf{ 7, 8, 9 }, METHOD_OVERWRITE, 3) == Buf{ 1, 1, 1, 7, 8 });
    CHECK(Apply(Buf(3, 1), Buf{ 7 }, METHOD_OVERWRITE, 10) == Buf{ 1, 1, 1 });  // offset past the end
}
