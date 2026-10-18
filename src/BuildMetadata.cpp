#include "BuildMetadata.h"
#include "Diagnostics.h"
#include "JsonString.h"
#include "Preferences.h"
#include "StandardOutput.h"
#include "StatusQuery.h"
#include <cstdio>

int WriteBuildInfo(std::string_view version, std::string_view revision, std::string_view sourceState) noexcept {
    char versionText[256], revisionText[256], sourceText[256];
    const auto quotedVersion = QuoteJsonString(version, versionText);
    const auto quotedRevision = QuoteJsonString(revision, revisionText);
    const auto quotedSource = QuoteJsonString(sourceState, sourceText);
    if (version.empty() || revision.empty() || sourceState.empty() || !quotedVersion || !quotedRevision || !quotedSource) {
        Diagnostics::Log("Build metadata contains an empty, invalid, or oversized identity field; no output was written.");
        return 1;
    }
    char output[1024];
    const int length = std::snprintf(output, sizeof(output),
        "{\"protocol\":%u,\"product\":\"RedLight\",\"version\":%.*s,\"build_revision\":%.*s,\"source_state\":%.*s,"
        "\"platform\":\"windows-x64\",\"profile_schema\":%u,\"status_protocol\":%u}\n",
        BuildMetadataProtocolVersion,
        static_cast<int>(quotedVersion->size()), quotedVersion->data(),
        static_cast<int>(quotedRevision->size()), quotedRevision->data(),
        static_cast<int>(quotedSource->size()), quotedSource->data(),
        PreferencesProfileVersion, REDLIGHT_STATUS_PROTOCOL);
    if (length < 0 || static_cast<std::size_t>(length) >= sizeof(output)) {
        Diagnostics::Log("Build metadata exceeded its output buffer; no output was written.");
        return 1;
    }
    if (WriteStandardOutput(std::string_view(output, static_cast<std::size_t>(length)))) return 0;
    Diagnostics::Log("Build metadata could not write standard output; discard any partial output.");
    return 1;
}
