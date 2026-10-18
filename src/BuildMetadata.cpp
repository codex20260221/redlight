#include "BuildMetadata.h"
#include "Diagnostics.h"
#include "JsonString.h"
#include "Preferences.h"
#include "StandardOutput.h"
#include "StatusQuery.h"
#include "PreferencesQuery.h"
#include <cstdio>
#include <span>
#include <charconv>

namespace {
std::optional<std::string_view> InputSchemaJson(std::span<char> output) noexcept {
    std::size_t length = 0;
    const auto put = [&](char value) { if (length == output.size()) return false; output[length++] = value; return true; };
    if (!put('[')) return std::nullopt;
    bool first = true;
    for (const auto schema : PreferencesInputSchemas) {
        if (!DefaultsForProfileSchema(schema) || (!first && !put(','))) return std::nullopt;
        const auto encoded = std::to_chars(output.data() + length, output.data() + output.size(), schema);
        if (encoded.ec != std::errc{}) return std::nullopt;
        length = static_cast<std::size_t>(encoded.ptr - output.data());
        first = false;
    }
    if (!put(']')) return std::nullopt;
    return std::string_view(output.data(), length);
}
}

int WriteBuildInfo(std::string_view version, std::string_view revision, std::string_view sourceState) noexcept {
    char versionText[256], revisionText[256], sourceText[256];
    char schemas[64];
    const auto inputSchemas = InputSchemaJson(schemas);
    const auto quotedVersion = QuoteJsonString(version, versionText);
    const auto quotedRevision = QuoteJsonString(revision, revisionText);
    const auto quotedSource = QuoteJsonString(sourceState, sourceText);
    if (version.empty() || revision.empty() || sourceState.empty() || !quotedVersion || !quotedRevision || !quotedSource || !inputSchemas) {
        Diagnostics::Log("Build metadata contains empty, invalid, or oversized identity/capability data; no output was written.");
        return 1;
    }
    char output[BuildMetadataJsonBufferSize];
    const int length = std::snprintf(output, sizeof(output),
        "{\"protocol\":%u,\"product\":\"RedLight\",\"version\":%.*s,\"build_revision\":%.*s,\"source_state\":%.*s,"
        "\"platform\":\"windows-x64\",\"profile_schema\":%u,\"status_protocol\":%u,\"current_settings_protocol\":%u,"
        "\"profile_input_schemas\":%.*s,\"limits\":{\"profile_bytes\":%zu,\"session_minutes\":%u}}\n",
        BuildMetadataProtocolVersion,
        static_cast<int>(quotedVersion->size()), quotedVersion->data(),
        static_cast<int>(quotedRevision->size()), quotedRevision->data(),
        static_cast<int>(quotedSource->size()), quotedSource->data(),
        PreferencesProfileVersion, REDLIGHT_STATUS_PROTOCOL, REDLIGHT_PREFERENCES_PROTOCOL,
        static_cast<int>(inputSchemas->size()), inputSchemas->data(), PreferencesProfileMaxBytes, SessionTimer::maxMinutes);
    if (length < 0 || static_cast<std::size_t>(length) >= sizeof(output)) {
        Diagnostics::Log("Build metadata exceeded its output buffer; no output was written.");
        return 1;
    }
    if (WriteStandardOutput(std::string_view(output, static_cast<std::size_t>(length)))) return 0;
    Diagnostics::Log("Build metadata could not write standard output; discard any partial output.");
    return 1;
}
