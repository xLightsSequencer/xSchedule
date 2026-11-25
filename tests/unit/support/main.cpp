// Test runner entry point.
//
// xSchedule's classes use wxWidgets types (wxDateTime, wxXmlNode, wxString), so the
// library is initialised once for the whole run. No wxApp and no windows: the tests
// run headless.

#include <catch2/catch_session.hpp>

#include <wx/init.h>

#include <cstdlib>
#include <log.h>

int main(int argc, char* argv[]) {
    wxInitializer wx;
    if (!wx.IsOk()) {
        return 1;
    }

    // The code under test logs freely; keep test output readable unless asked for.
    const char* lvl = std::getenv("XS_TEST_LOG");
    spdlog::set_level(lvl != nullptr ? spdlog::level::from_str(lvl) : spdlog::level::off);

    return Catch::Session().run(argc, argv);
}
