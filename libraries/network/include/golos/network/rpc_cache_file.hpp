#pragma once
#include <string>
#include <cstdio>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace golos { namespace network {
// Owner creates pending in the target directory. No copy/remove-target fallback:
// a failed publication preserves the old target and removes only our pending file.
#ifdef _WIN32
inline bool atomic_replace(const std::wstring& pending, const std::wstring& target) {
    if (MoveFileExW(pending.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
    DeleteFileW(pending.c_str());
    return false;
}
#else
inline bool atomic_replace(const std::string& pending, const std::string& target) {
    if (::rename(pending.c_str(), target.c_str()) == 0) return true;
    ::remove(pending.c_str());
    return false;
}
#endif
}}
