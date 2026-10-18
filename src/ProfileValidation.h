#pragma once

// Read-only bounded disk-file validation. 0 valid, 4 invalid, 1 I/O/output failure.
int ValidateSettingsProfileFile(const wchar_t* path) noexcept;
