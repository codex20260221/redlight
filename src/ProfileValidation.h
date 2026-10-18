#pragma once

// Read-only bounded disk-file validation with schema/default/resolved-preference JSON.
// 0 valid, 4 invalid, 1 I/O/output failure.
int ValidateSettingsProfileFile(const wchar_t* path) noexcept;

// Read-only input, canonical current-format profile on stdout; no settings import.
// 0 complete output, 4 invalid data, 1 file/serialization/output failure.
int NormalizeSettingsProfileFile(const wchar_t* path) noexcept;

// Validate a bounded read-only input, then compare with saved/default preferences.
// JSON output. 0 match, 5 different, 4 invalid data, 1 file/settings/output failure.
int CompareSettingsProfileFile(const wchar_t* path) noexcept;

// Validate/close bounded input before an explicit guarded next-start preference save.
// JSON output. 0 saved, 6 instance present, 4 invalid data, 1 I/O/storage/guard/output failure.
int SaveSettingsProfileFile(const wchar_t* path) noexcept;
