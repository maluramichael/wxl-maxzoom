// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <windows.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>

namespace wxl_maxzoom::settings
{
    // Resolve the module's directory even when the launcher sets a different working directory.
    inline std::wstring IniPath()
    {
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                reinterpret_cast<LPCWSTR>(&IniPath), &module))
            return {};

        std::wstring path(MAX_PATH, L'\0');
        for (;;)
        {
            const DWORD length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
            if (length == 0)
                return {};
            if (length < path.size())
            {
                path.resize(length);
                return std::filesystem::path(path).replace_filename(L"wxl-maxzoom.ini").wstring();
            }
            path.resize(path.size() * 2);
        }
    }

    inline float Read(const std::wstring& path, const wchar_t* key, float fallback, float low, float high)
    {
        if (path.empty())
            return fallback;
        constexpr DWORD capacity = 64;
        wchar_t text[capacity];
        const DWORD count = GetPrivateProfileStringW(L"MaxZoom", key, L"", text, capacity, path.c_str());
        if (count == 0 || count == capacity - 1)
            return fallback;

        std::wistringstream input(text);
        input.imbue(std::locale::classic());
        float value;
        if (!(input >> value) || !std::isfinite(value))
            return fallback;
        input >> std::ws;
        return input.eof() ? std::clamp(value, low, high) : fallback;
    }

    inline bool Write(const std::wstring& path, const wchar_t* key, float value)
    {
        if (path.empty())
            return false;
        std::wostringstream text;
        text.imbue(std::locale::classic());
        text << std::setprecision(std::numeric_limits<float>::max_digits10) << value;
        return WritePrivateProfileStringW(L"MaxZoom", key, text.str().c_str(), path.c_str()) != FALSE;
    }
}
