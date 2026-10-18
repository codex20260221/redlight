#pragma once
#include <string_view>

// Use inherited redirection or an existing parent console; never create a console.
bool WriteStandardOutput(std::string_view text) noexcept;
