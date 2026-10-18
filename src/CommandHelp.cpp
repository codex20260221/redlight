#include "CommandHelp.h"
#include "StandardOutput.h"
#include "Diagnostics.h"

int WriteCommandLineHelp(std::string_view version) noexcept {
    char output[CommandHelpBufferSize];
    const auto text = FormatCommandLineHelp(version, output);
    if (!text) {
        Diagnostics::Log("Command-line help could not be formatted completely; no output was written.");
        return 1;
    }
    if (WriteStandardOutput(*text)) return 0;
    Diagnostics::Log("Command-line help could not write standard output; discard partial output.");
    return 1;
}
