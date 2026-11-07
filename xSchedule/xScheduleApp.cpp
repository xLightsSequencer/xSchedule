/***************************************************************
 * This source files comes from the xLights project
 * https://www.xlights.org
 * https://github.com/xLightsSequencer/xLights
 * See the github commit history for a record of contributing
 * developers.
 * Copyright claimed based on commit dates recorded in Github
 * License: https://github.com/xLightsSequencer/xLights/blob/master/License.txt
 **************************************************************/

#include "xScheduleApp.h"

//(*AppHeaders
#include "xScheduleMain.h"
#include <map>
#include <wx/image.h>
//*)

#include <log.h>
#include "LogSink.h"
#include "Diagnostics.h"
#include "spdlog/common.h"

#include <SpecialOptions.h>

#include <wx/file.h>
#include <wx/msgdlg.h>

#include "ScheduleManager.h"
#include "../xlights/src-core/outputs/OutputManager.h"
#include "../xlights/src-core/xLightsVersion.h"
#include "xScheduleVersion.h"
#include "../xlights/src-core/utils/UtilFunctions.h"
#include "ScheduleConfig.h"
#include "../xlights/src-core/settings/XLightsSettings.h"
#include "ModernUI.h"
#include <wx/dialog.h>
#include "../xlights/src-ui-wx/shared/utils/wxUtilities.h"
#include "../xlights/src-core/utils/AppCallbacks.h"
#include <wx/cmdline.h>
#include <wx/confbase.h>
#include <wx/debugrpt.h>
#include <wx/filename.h>
#include <wx/snglinst.h>
#include <wx/stdpaths.h>

#ifdef _MSC_VER

    #pragma warning(push)
    #pragma warning(disable : 4081)

    #ifdef _DEBUG
        #pragma comment(lib, "wxbase"WXWIDGETS_VERSION"ud.lib")
        #pragma comment(lib, "wxbase"WXWIDGETS_VERSION"ud_net.lib")
        #pragma comment(lib, "wxmsw"WXWIDGETS_VERSION"ud_core.lib")
        #pragma comment(lib, "wxscintillad.lib")
        #pragma comment(lib, "wxregexud.lib")
        #pragma comment(lib, "wxbase"WXWIDGETS_VERSION"ud_xml.lib")
        #pragma comment(lib, "wxtiffd.lib")
        #pragma comment(lib, "wxjpegd.lib")
        #pragma comment(lib, "wxpngd.lib")
        #pragma comment(lib, "wxzlibd.lib")
        #pragma comment(lib, "wxwebpd.lib")
        #pragma comment(lib, "wxmsw" WXWIDGETS_VERSION "ud_qa.lib")
        #pragma comment(lib, "wxexpatd.lib")
        #pragma comment(lib, "msvcprtd.lib")
        #pragma comment(lib, "libzstdd_static_VS.lib")


    #else
        #pragma comment(lib, "wxbase"WXWIDGETS_VERSION"u.lib")
        #pragma comment(lib, "wxbase"WXWIDGETS_VERSION"u_net.lib")
        #pragma comment(lib, "wxmsw"WXWIDGETS_VERSION"u_core.lib")
        #pragma comment(lib, "wxscintilla.lib")
        #pragma comment(lib, "wxregexu.lib")
        #pragma comment(lib, "wxbase"WXWIDGETS_VERSION"u_xml.lib")
        #pragma comment(lib, "wxtiff.lib")
        #pragma comment(lib, "wxjpeg.lib")
        #pragma comment(lib, "wxpng.lib")
        #pragma comment(lib, "wxzlib.lib")
        #pragma comment(lib, "wxwebp.lib")
        #pragma comment(lib, "wxmsw" WXWIDGETS_VERSION "u_qa.lib")
        #pragma comment(lib, "wxexpat.lib")
        #pragma comment(lib, "libzstd_static_VS.lib")
        #pragma comment(lib, "msvcprt.lib")
    #endif

    #pragma comment(lib, "libcurl.lib")
    #pragma comment(lib, "z.lib")
    #pragma comment(lib, "iphlpapi.lib")
    #pragma comment(lib, "WS2_32.Lib")
    #pragma comment(lib, "comdlg32.lib")
    #pragma comment(lib, "comctl32.lib")
    #pragma comment(lib, "Rpcrt4.lib")
    #pragma comment(lib, "uuid.lib")
    #pragma comment(lib, "advapi32.lib")
    #pragma comment(lib, "shell32.lib")
    #pragma comment(lib, "ole32.lib")
    #pragma comment(lib, "oleaut32.lib")
    #pragma comment(lib, "odbc32.lib")
    #pragma comment(lib, "odbccp32.lib")
    #pragma comment(lib, "kernel32.lib")
    #pragma comment(lib, "user32.lib")
    #pragma comment(lib, "winspool.lib")
    #pragma comment(lib, "gdi32.lib")
    #pragma comment(lib, "winmm.lib")
    #pragma comment(lib, "ImageHlp.Lib")
    #pragma comment(lib, "avcodec.lib")
    #pragma comment(lib, "avdevice.lib")
    #pragma comment(lib, "avfilter.lib")
    #pragma comment(lib, "avformat.lib")
    #pragma comment(lib, "avutil.lib")
    #pragma comment(lib, "swresample.lib")
    #pragma comment(lib, "swscale.lib")
    #pragma comment(lib, "SDL2.lib")

    #pragma warning(pop)

#endif

IMPLEMENT_APP(xScheduleApp)

xScheduleApp::xScheduleApp() :
    xlBaseApp("xSchedule")
{
}

std::string DecodeOS(wxOperatingSystemId o)
{
    switch (o) {
    case wxOS_UNKNOWN:
        return "Call get get operating system failed.";
    case wxOS_MAC_OS:
        return "Apple Mac OS 8 / 9 / X with Mac paths.";
    case wxOS_MAC_OSX_DARWIN:
        return "Apple OS X with Unix paths.";
    case wxOS_MAC:
        return "An Apple Mac of some type.";
    case wxOS_WINDOWS_NT:
        return "Windows NT family(XP / Vista / 7 / 8 / 10).";
    case wxOS_WINDOWS:
        return "A Windows system of some type.";
    case wxOS_UNIX_LINUX:
        return "Linux.";
    case wxOS_UNIX_FREEBSD:
        return "FreeBSD.";
    case wxOS_UNIX_OPENBSD:
        return "OpenBSD.";
    case wxOS_UNIX_NETBSD:
        return "NetBSD.";
    case wxOS_UNIX_SOLARIS:
        return "Solaris.";
    case wxOS_UNIX_AIX:
        return "AIX.";
    case wxOS_UNIX_HPUX:
        return "HP / UX.";
    case wxOS_UNIX:
        return "Some flavour of Unix.";
    default:
        break;
    }

    return "Unknown Operating System.";
}

void DumpConfig()
{
    spdlog::info("Version: " + xschedule_version_string);
    spdlog::info("Bits: " + GetBitness());
    spdlog::info("Build Date: " + xschedule_build_date);
    spdlog::info("WX Version: " + std::string(wxString(wxVERSION_STRING).c_str()));

    spdlog::info("Machine configuration:");
    spdlog::info("  Total memory: " + std::to_string(GetPhysicalMemorySizeMB()) + " MB");
    wxMemorySize s = wxGetFreeMemory();
    if (s != -1) {
#if wxUSE_LONGLONG
        wxString msg = wxString::Format(_T("  Free Memory: %" wxLongLongFmtSpec "d."), s);
#else
        wxString msg = wxString::Format(_T("  Free Memory: %ld."), s);
#endif
        spdlog::info(msg.ToStdString());
    }
    spdlog::info("  Current directory: " + std::string(wxGetCwd().c_str()));
    spdlog::info("  Machine name: " + std::string(wxGetHostName().c_str()));
    spdlog::info("  OS: " + std::string(wxGetOsDescription().c_str()));
    int verMaj = -1;
    int verMin = -1;
    wxOperatingSystemId o = wxGetOsVersion(&verMaj, &verMin);
    spdlog::info("  OS: {} {}.{}", (const char*)DecodeOS(o).c_str(), verMaj, verMin);
    if (wxIsPlatform64Bit()) {
        spdlog::info("      64 bit");
    } else {
        spdlog::info("      NOT 64 bit");
    }
    if (wxIsPlatformLittleEndian()) {
        spdlog::info("      Little Endian");
    } else {
        spdlog::info("      Big Endian");
    }
    spdlog::info("  CPU Arch: {}", wxGetCpuArchitectureName().ToStdString());

#ifdef LINUX
    wxLinuxDistributionInfo l = wxGetLinuxDistributionInfo();
    spdlog::info("  " + std::string(l.Id.c_str()) + " " + std::string(l.Release.c_str()) + " " + std::string(l.CodeName.c_str()) + " " + std::string(l.Description.c_str()));
#endif
}

static std::shared_ptr<LogSink> __logSink;

std::string GetXScheduleLogPath()
{
    return __logSink != nullptr ? __logSink->GetPath() : std::string();
}

void InitialiseLogging(bool fromMain)
{
    static bool loggingInitialised = false;

    if (!loggingInitialised) {
        std::string const logFileName = "xschedule_spdlog.log";
#ifdef __WXMSW__
        wxString dir;
        wxGetEnv("APPDATA", &dir);
        std::string const logFilePath = std::string(dir.c_str()) + "\\" + logFileName;
#endif
#ifdef __WXOSX__
        wxFileName home;
        home.AssignHomeDir();
        wxString const dir = home.GetFullPath();
        std::string const logFilePath = std::string(dir.c_str()) + "/Library/Logs/" + logFileName;
#endif
#ifdef __LINUX__
        std::string const logFilePath = "/tmp/" + logFileName;
#endif

        // wxStandardPaths::Get().Get()

        __logSink = std::make_shared<LogSink>(logFilePath, 10 * 1024 * 1024, 7, 30 * 1024 * 1024);
        Diagnostics::NameThread("main");

        auto file_logger = std::make_shared<spdlog::logger>("xschedule", __logSink);
        auto curl_logger = std::make_shared<spdlog::logger>("curl", __logSink);
        auto frame_logger = std::make_shared<spdlog::logger>("frame", __logSink);
        auto job_logger = std::make_shared<spdlog::logger>("job", __logSink);
        curl_logger->set_level(spdlog::level::from_str(SpecialOptions::GetOption("curl_logger", "info")));
        frame_logger->set_level(spdlog::level::from_str(SpecialOptions::GetOption("frame_logger", "info")));
        spdlog::register_logger(curl_logger);
        spdlog::register_logger(frame_logger);
        spdlog::register_logger(job_logger);

        loggingInitialised = true;
        spdlog::initialize_logger(file_logger);
        spdlog::set_default_logger(file_logger);
        spdlog::set_formatter(Diagnostics::MakeFormatter("%Y-%m-%d %H:%M:%S.%e [%l] [%N] %v"));

        // wxOperatingSystemId os = wxGetOsVersion();
        // std::string osStr = DecodeOS(os);
        // std::string initFileName;

        wxDateTime now = wxDateTime::Now();
        int millis = wxGetUTCTimeMillis().GetLo() % 1000;
        wxString ts = wxString::Format("%04d-%02d-%02d_%02d-%02d-%02d-%03d", now.GetYear(), now.GetMonth() + 1, now.GetDay(), now.GetHour(), now.GetMinute(), now.GetSecond(), millis);
        // spdlog::info("Start Time: {}.", ts);

        spdlog::info("Start Time: {}.", ts.ToStdString());
        spdlog::info("Current working directory {}.", wxGetCwd().ToStdString());
    }   
}

void xScheduleApp::WipeSettings()
{
    spdlog::info("Wiping settings.");

    ScheduleConfig::Wipe();
}

int xScheduleApp::OnExit()
{
    if (_checker != nullptr) {
        delete _checker;
        _checker = nullptr;
    }
    spdlog::info("xSchedule exiting.");
    XLightsSettings::Get().Flush();
    Diagnostics::StopWatchdog();
    if (__logSink != nullptr) {
        __logSink->Stop();
    }

    return 0;
}

int xScheduleApp::FilterEvent(wxEvent& event)
{
    // every dialog opens big enough for its contents, whatever display scale its saved size came from
    if (event.GetEventType() == wxEVT_SHOW && static_cast<wxShowEvent&>(event).IsShown()) {
        if (auto dlg = dynamic_cast<wxDialog*>(event.GetEventObject()); dlg != nullptr) {
            ModernUI::FitListHeaders(dlg);
            ModernUI::GrowFilePickers(dlg);
            ModernUI::FitToContents(dlg);
            dlg->CallAfter([dlg]() { ModernUI::LogClippedControls(dlg); });
        }
    }
    return xlBaseApp::FilterEvent(event);
}

bool xScheduleApp::OnInit()
{
    _checker = nullptr;

    // seed the random number generator
    srand(wxGetLocalTimeMillis().GetLo());

    wxLog::SetLogLevel(wxLOG_FatalError);

    // curl_global_init(CURL_GLOBAL_SSL);

    InitialiseLogging(false);
    spdlog::info("******* OnInit: xSchedule started.");
    Diagnostics::StartWatchdog();

    // The wx-free xLights core reaches the app through these hooks. Without them "main thread" work runs on
    // the calling thread, errors are only logged, and a worker thread exception terminates.
    AppCallbacks::SetPostToMainThread([](std::function<void()> fn) {
        wxTheApp->CallAfter(std::move(fn));
    });
    AppCallbacks::SetHandleUnhandledException([] {
        wxTheApp->OnUnhandledException();
    });
    AppCallbacks::SetSetupThreadCrashHandler([] {
        xlCrashHandler::SetupCrashHandlerForNonWxThread();
    });
    AppCallbacks::SetDisplayMessageCallback([](AppCallbacks::DisplayMessageLevel level, const std::string& msg) {
        // already logged by the core; see ScheduleManager::StartOutputToLights
        if (ScheduleManager::IsStartingOutputToLights()) return;
        // Shown after the caller returns: a box opened inside a frame tick stops that timer until someone clicks
        // OK, and an unattended machine would collect boxes inside boxes. One at a time, and not the same one again
        // for 10 minutes; the core has logged them all.
        auto showBox = [level, msg]() {
            static bool showing = false;
            static std::map<std::string, time_t> shown;
            if (showing) return;
            auto last = shown.find(msg);
            if (last != shown.end() && time(nullptr) - last->second < 600) return;
            shown[msg] = time(nullptr);
            showing = true;
            switch (level) {
            case AppCallbacks::DisplayMessageLevel::Error:
                wxMessageBox(msg, "Error", wxICON_ERROR | wxOK);
                break;
            case AppCallbacks::DisplayMessageLevel::Warning:
                wxMessageBox(msg, "Warning", wxICON_WARNING | wxOK);
                break;
            case AppCallbacks::DisplayMessageLevel::Info:
                wxMessageBox(msg, "Information", wxICON_INFORMATION | wxOK);
                break;
            }
            showing = false;
        };
        AppCallbacks::PostToMainThread(showBox);
    });

    // the core finds the controller definitions here
    spdlog::info("Resources folder: {}", GetResourcesDirectory());

    ScheduleConfig::Initialize();

#ifdef __WXMSW__
    spdlog::debug("xSchedule module handle {:#x}", reinterpret_cast<uintptr_t>(::GetModuleHandle(nullptr)));
    spdlog::debug("xSchedule wxTheApp {:#x}", reinterpret_cast<uintptr_t>(wxTheApp));
    if (!ScheduleConfig::IsSuppressDarkMode()) {
        MSWEnableDarkMode();
    }
#endif

    DumpConfig();

    static const wxCmdLineEntryDesc cmdLineDesc[] = {
        { wxCMD_LINE_SWITCH, "h", "help", "displays help on the command line parameters", wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
        { wxCMD_LINE_OPTION, "s", "show", "specify show directory" },
        { wxCMD_LINE_OPTION, "p", "playlist", "specify the playlist to play" },
        { wxCMD_LINE_SWITCH, "w", "wipe", "wipe settings clean" },
        { wxCMD_LINE_SWITCH, "x", "exit", "exit silently if another instance is runnning" },
        { wxCMD_LINE_NONE }
    };

    bool parmfound = false;
    bool wipeSettings = false;
    bool exitIfRunning = false;
    wxString showDir;
    wxString playlist;
    wxCmdLineParser parser(cmdLineDesc, argc, argv);
    switch (parser.Parse()) {
    case -1:
        // help was given
        return false;
    case 0:
        if (parser.Found("w")) {
            parmfound = true;
            spdlog::info("-w: Wiping settings.");
            WipeSettings();
            wipeSettings = true;
        }
        if (parser.Found("x")) {
            spdlog::info("-x: Exit silently if another instance is running.");
            exitIfRunning = true;
        }
        if (parser.Found("s", &showDir)) {
            parmfound = true;
            spdlog::info("-s: Show directory set to {}.", (const char*)showDir.c_str());
        }
        if (parser.Found("p", &playlist)) {
            parmfound = true;
            spdlog::info("-p: Playlist to play {}.", (const char*)playlist.c_str());
        }
        if (!parmfound && parser.GetParamCount() > 0) {
            spdlog::info("Unrecognised command line parameter found.");
            wxMessageBox("Unrecognised command line parameter found.", _("Command Line Options")); // give positive feedback*/
        }
        break;
    default:
        wxMessageBox(_("Unrecognized command line parameters"), _("Command Line Error"));
        return false;
    }

    _checker = new wxSingleInstanceChecker();
    if (showDir == "") {
        if (_checker->IsAnotherRunning()) {
            spdlog::info("Another instance of xSchedule is running. Exiting.");
            delete _checker; // OnExit() won't be called if we return false
            _checker = nullptr;

            // WOuld be nice to switch focuse here to the existing instance ... but that doesnt work ... this only sees windows in this process
            // wxWindow* x = FindWindowByLabel(_("xLights Scheduler"));

            if (!exitIfRunning) {
                wxMessageBox("Another instance of xSchedule is already running. A second instance not allowed. Exiting.");
            }

            return false;
        }
    } else {
        _checker->CreateDefault();
    }

    //(*AppInitialize
    bool wxsOK = true;
    wxInitAllImageHandlers();
    if (wxsOK) {
        xScheduleFrame* Frame = new xScheduleFrame(0, showDir, playlist);
        Frame->Show();
        SetTopWindow(Frame);
        if (wipeSettings)
            Frame->GetPluginManager().WipeSettings();
    }
    //*)
    return wxsOK;
}
