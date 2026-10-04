// A failing test must write its failure to the output, never open a dialog that waits for a click: on a workstation it
// blocks the whole run, and on CI it hangs until the timeout.

#ifdef _MSC_VER

// clang-format off
#include <windows.h>
#include <dbghelp.h>
// clang-format on

#include <crtdbg.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

// Win32 and CRT interop: C APIs, raw buffers and a C-style report hook, which clang-tidy rightly dislikes elsewhere.
// NOLINTBEGIN
namespace {

#ifdef _DEBUG

// An assertion of the debug CRT or STL is about to be reported: write where it comes from, which the message alone
// (a line of <vector>) does not say.
int writeStackOnReport(int /*reportType*/, char* /*message*/, int* /*returnValue*/)
{
    constexpr int kFrames = 32;
    std::array<void*, kFrames> frames{};
    const HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, nullptr, TRUE);
    const USHORT count = CaptureStackBackTrace(0, kFrames, frames.data(), nullptr);
    for (USHORT index = 0; index < count; ++index) {
        const auto address = reinterpret_cast<DWORD64>(frames[index]);
        alignas(SYMBOL_INFO) std::array<char, sizeof(SYMBOL_INFO) + MAX_SYM_NAME> buffer{};
        auto* const symbol = reinterpret_cast<SYMBOL_INFO*>(buffer.data());
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;
        if (SymFromAddr(process, address, &displacement, symbol) == 0) {
            continue;
        }
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisplacement = 0;
        if (SymGetLineFromAddr64(process, address, &lineDisplacement, &line) != 0) {
            std::fprintf(stderr, "  at %s (%s:%lu)\n", symbol->Name, line.FileName, line.LineNumber);
        } else {
            std::fprintf(stderr, "  at %s\n", symbol->Name);
        }
    }
    return FALSE; // let the CRT report it as usual
}

#endif

struct NoDialogs {
    NoDialogs()
    {
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
        // Abort writes its message instead of showing a box, and does not call the Windows fault reporter
        _set_abort_behavior(_WRITE_ABORT_MSG, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#ifdef _DEBUG
        // Debug CRT and debug STL assertions ("Debug Assertion Failed!") go to stderr, with their call stack
        for (const int type : {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT}) {
            _CrtSetReportMode(type, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(type, _CRTDBG_FILE_STDERR);
        }
        _CrtSetReportHook(writeStackOnReport);
#endif
    }
};

const NoDialogs noDialogs;

} // namespace
// NOLINTEND

#endif
