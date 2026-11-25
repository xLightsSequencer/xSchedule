// Output processing: the per-frame channel manipulations configured in
// "Output Processing" (dim, remap, colour order, gamma, ...).

#include <catch2/catch_test_macros.hpp>

#include "OutputProcess.h"
#include "OutputProcessColourOrder.h"
#include "OutputProcessDeadChannel.h"
#include "OutputProcessDim.h"
#include "OutputProcessDimWhite.h"
#include "OutputProcessExcludeDim.h"
#include "OutputProcessGamma.h"
#include "OutputProcessRemap.h"
#include "OutputProcessReverse.h"
#include "OutputProcessSet.h"
#include "OutputProcessSustain.h"
#include "OutputProcessThreeToFour.h"

#include "outputs/OutputManager.h"

#include <wx/xml/xml.h>

#include <cmath>
#include <list>
#include <memory>
#include <numeric>
#include <vector>

namespace {
using Buf = std::vector<uint8_t>;

// Plain absolute start channels ("1", "4", ...) decode without any controllers.
OutputManager& OM() {
    static OutputManager om;
    return om;
}

void Run(OutputProcess& op, Buf& b, std::list<OutputProcess*> processes = {}) {
    processes.push_back(&op);
    op.Frame(b.data(), b.size(), processes);
}

Buf Ramp(size_t n, uint8_t first = 10) {
    Buf b(n);
    std::iota(b.begin(), b.end(), first);
    return b;
}
} // namespace

TEST_CASE("Set fills channels with a value", "[outputprocess]") {
    Buf b(8, 1);
    OutputProcessSet op(&OM(), "3", 4, 99, "");
    Run(op, b);
    CHECK(b == Buf{ 1, 1, 99, 99, 99, 99, 1, 1 });

    SECTION("stops at the end of the buffer") {
        Buf c(5, 1);
        OutputProcessSet tail(&OM(), "4", 10, 7, "");
        Run(tail, c);
        CHECK(c == Buf{ 1, 1, 1, 7, 7 });
    }
}

TEST_CASE("Remap copies a channel block", "[outputprocess]") {
    Buf b = Ramp(8);  // 10..17
    OutputProcessRemap op(&OM(), "2", 6, 3, "");  // channels 2-4 -> 6-8
    Run(op, b);
    CHECK(b == Buf{ 10, 11, 12, 13, 14, 11, 12, 13 });
}

TEST_CASE("Dim scales channel values", "[outputprocess]") {
    Buf b{ 200, 100, 50, 255 };

    SECTION("50%") {
        OutputProcessDim op(&OM(), "1", 3, 50, "");
        Run(op, b);
        CHECK(b == Buf{ 100, 50, 25, 255 });  // only the configured channels
    }
    SECTION("0% blanks") {
        OutputProcessDim op(&OM(), "2", 2, 0, "");
        Run(op, b);
        CHECK(b == Buf{ 200, 0, 0, 255 });
    }
    SECTION("100% and disabled leave the data alone") {
        OutputProcessDim full(&OM(), "1", 4, 100, "");
        Run(full, b);
        OutputProcessDim off(&OM(), "1", 4, 10, "");
        off.Enable(false);
        Run(off, b);
        CHECK(b == Buf{ 200, 100, 50, 255 });
    }
}

TEST_CASE("Dim skips excluded channels", "[outputprocess][excludedim]") {
    SECTION("dim starting at channel 1") {
        Buf b(6, 200);
        OutputProcessDim dim(&OM(), "1", 6, 50, "");
        OutputProcessExcludeDim ex(&OM(), "3", 2, "");
        Run(dim, b, { &ex });
        CHECK(b == Buf{ 100, 100, 200, 200, 100, 100 });
    }
    SECTION("several exclusion ranges") {
        Buf b(12, 200);
        OutputProcessDim dim(&OM(), "1", 12, 50, "");
        OutputProcessExcludeDim ex1(&OM(), "2", 1, "");
        OutputProcessExcludeDim ex2(&OM(), "10", 2, "");
        Run(dim, b, { &ex1, &ex2 });
        CHECK(b == Buf{ 100, 200, 100, 100, 100, 100, 100, 100, 100, 200, 200, 100 });
    }
}

TEST_CASE("Dim with exclusions starting part way through the buffer", "[outputprocess][excludedim]") {
    Buf b(10, 200);
    OutputProcessDim dim(&OM(), "3", 6, 50, "");   // channels 3-8
    OutputProcessExcludeDim ex(&OM(), "5", 2, "");  // keep 5-6
    Run(dim, b, { &ex });
    CHECK(b == Buf{ 200, 200, 100, 100, 200, 200, 100, 100, 200, 200 });
}

TEST_CASE("Dim honours every exclusion range regardless of the order they were added", "[outputprocess][excludedim]") {
    Buf b(8, 200);
    OutputProcessDim dim(&OM(), "1", 8, 50, "");
    OutputProcessExcludeDim ex1(&OM(), "2", 1, "");
    OutputProcessExcludeDim ex2(&OM(), "5", 1, "");
    Run(dim, b, { &ex1, &ex2 });
    CHECK(b == Buf{ 100, 200, 100, 100, 200, 100, 100, 100 });
}

TEST_CASE("Dim white only dims white pixels", "[outputprocess]") {
    Buf b{ 200, 200, 200,   200, 100, 200,   80, 80, 80 };
    OutputProcessDimWhite op(&OM(), "1", 3, 50, "");
    Run(op, b);
    CHECK(b == Buf{ 100, 100, 100,   200, 100, 200,   40, 40, 40 });
}

TEST_CASE("Colour order swaps channels in each node", "[outputprocess]") {
    struct C { int order; Buf expected; };
    for (const auto& c : { C{ 123, { 1, 2, 3, 4, 5, 6 } },
                           C{ 132, { 1, 3, 2, 4, 6, 5 } },
                           C{ 213, { 2, 1, 3, 5, 4, 6 } },
                           C{ 231, { 2, 3, 1, 5, 6, 4 } },
                           C{ 312, { 3, 1, 2, 6, 4, 5 } },
                           C{ 321, { 3, 2, 1, 6, 5, 4 } } }) {
        CAPTURE(c.order);
        Buf b{ 1, 2, 3, 4, 5, 6 };
        OutputProcessColourOrder op(&OM(), "1", 2, c.order, "");
        Run(op, b);
        CHECK(b == c.expected);
    }
}

TEST_CASE("Reverse flips node order", "[outputprocess]") {
    SECTION("odd node count") {
        Buf b{ 1, 2, 3,   4, 5, 6,   7, 8, 9 };
        OutputProcessReverse op(&OM(), "1", 3, 0, "");
        Run(op, b);
        CHECK(b == Buf{ 7, 8, 9,   4, 5, 6,   1, 2, 3 });
    }
    SECTION("even node count, offset start") {
        Buf b{ 0,   1, 2, 3,   4, 5, 6 };
        OutputProcessReverse op(&OM(), "2", 2, 0, "");
        Run(op, b);
        CHECK(b == Buf{ 0,   4, 5, 6,   1, 2, 3 });
    }
}

TEST_CASE("Gamma correction", "[outputprocess]") {
    auto expect = [](int v, double g) { return (uint8_t)(255 * std::pow(v / 255.0, g)); };

    SECTION("single gamma for all channels") {
        Buf b{ 0, 64, 128,   200, 255, 10 };
        OutputProcessGamma op(&OM(), "1", 2, 2.2f, 1.0f, 1.0f, 1.0f, "");
        Run(op, b);
        CHECK(b == Buf{ 0, expect(64, 2.2f), expect(128, 2.2f), expect(200, 2.2f), 255, expect(10, 2.2f) });
    }
    SECTION("separate red/green/blue gamma when the overall gamma is 0") {
        Buf b{ 128, 128, 128 };
        OutputProcessGamma op(&OM(), "1", 1, 0.0f, 2.0f, 1.0f, 0.5f, "");
        Run(op, b);
        CHECK(b == Buf{ expect(128, 2.0f), 128, expect(128, 0.5f) });
    }
    SECTION("gamma 1 is a no-op") {
        Buf b{ 1, 2, 3 };
        OutputProcessGamma op(&OM(), "1", 1, 1.0f, 2.0f, 2.0f, 2.0f, "");
        Run(op, b);
        CHECK(b == Buf{ 1, 2, 3 });
    }
}

TEST_CASE("Three to four channel expansion", "[outputprocess]") {
    SECTION("RGBW") {
        Buf b{ 10, 20, 30,   50, 50, 50,   0, 0, 0,   0, 0, 0 };
        OutputProcessThreeToFour op(&OM(), "1", 3, "RGBW", "");
        Run(op, b);
        CHECK(b == Buf{ 10, 20, 30, 0,   0, 0, 0, 50,   0, 0, 0, 0 });
    }
}

TEST_CASE("Three to four channel expansion, WRGB", "[outputprocess]") {
    Buf b{ 10, 20, 30,   50, 50, 50,   0, 0 };
    OutputProcessThreeToFour op(&OM(), "1", 2, "WRGB", "");
    Run(op, b);
    CHECK(b == Buf{ 0, 10, 20, 30,   50, 0, 0, 0 });
}

TEST_CASE("Sustain holds the last non-zero value", "[outputprocess]") {
    OutputProcessSustain op(&OM(), "2", 2, "");
    Buf f1{ 1, 5, 6, 1 };
    Run(op, f1);
    CHECK(f1 == Buf{ 1, 5, 6, 1 });

    Buf f2{ 0, 0, 9, 0 };
    Run(op, f2);
    CHECK(f2 == Buf{ 0, 5, 9, 0 });  // channel 1 and 4 are outside the range

    Buf f3{ 0, 0, 0, 0 };
    Run(op, f3);
    CHECK(f3 == Buf{ 0, 5, 9, 0 });
}

TEST_CASE("Dead channel blanks the node", "[outputprocess]") {
    OutputProcessDeadChannel op(&OM(), "4", 2, "");  // node at 4-6, green is dead

    Buf on{ 9, 9, 9,   10, 20, 30 };
    Run(op, on);
    CHECK(on == Buf{ 9, 9, 9,   0, 0, 0 });

    Buf off{ 9, 9, 9,   10, 0, 30 };
    Run(op, off);
    CHECK(off == Buf{ 9, 9, 9,   10, 0, 30 });
}

TEST_CASE("Output processes survive save and reload", "[outputprocess][xml]") {
    std::vector<std::unique_ptr<OutputProcess>> all;
    all.emplace_back(new OutputProcessDim(&OM(), "5", 10, 40, "dim"));
    all.emplace_back(new OutputProcessExcludeDim(&OM(), "7", 2, "exclude"));
    all.emplace_back(new OutputProcessDimWhite(&OM(), "1", 30, 60, "dimwhite"));
    all.emplace_back(new OutputProcessThreeToFour(&OM(), "1", 30, "WRGB", "3to4"));
    all.emplace_back(new OutputProcessSet(&OM(), "11", 4, 255, "set"));
    all.emplace_back(new OutputProcessSustain(&OM(), "2", 8, "sustain"));
    all.emplace_back(new OutputProcessRemap(&OM(), "3", 40, 6, "remap"));
    all.emplace_back(new OutputProcessReverse(&OM(), "1", 50, 0, "reverse"));
    all.emplace_back(new OutputProcessColourOrder(&OM(), "1", 50, 321, "order"));
    all.emplace_back(new OutputProcessGamma(&OM(), "1", 50, 2.2f, 1.0f, 1.0f, 1.0f, "gamma"));
    all.emplace_back(new OutputProcessDeadChannel(&OM(), "13", 3, "dead"));

    for (auto& op : all) {
        CAPTURE(op->GetType());
        op->Enable(false);
        std::unique_ptr<wxXmlNode> node(op->Save());
        std::unique_ptr<OutputProcess> back(OutputProcess::CreateFromXml(&OM(), node.get()));
        REQUIRE(back != nullptr);
        CHECK(back->GetType() == op->GetType());
        CHECK(back->GetStartChannel() == op->GetStartChannel());
        CHECK(back->GetDescription() == op->GetDescription());
        CHECK(back->GetP1() == op->GetP1());
        CHECK(back->GetP2() == op->GetP2());
        CHECK_FALSE(back->IsEnabled());
    }
}

TEST_CASE("Unknown output process node", "[outputprocess][xml]") {
    wxXmlNode node(nullptr, wxXML_ELEMENT_NODE, "OPNotAThing");
    CHECK(OutputProcess::CreateFromXml(&OM(), &node) == nullptr);
}
