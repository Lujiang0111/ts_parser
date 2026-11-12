#include <vector>
#include <windows.h>
#include "app/utils.h"

namespace app
{

std::string Utf16ToUtf8(const std::wstring &utf16)
{
    if (utf16.empty())
    {
        return "";
    }

    // 计算 UTF-8 所需字节数
    int size = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        utf16.data(),
        static_cast<int>(utf16.size()),
        nullptr,
        0,
        nullptr,
        nullptr);

    if (size <= 0)
    {
        return "";
    }

    // 分配空间
    std::vector<char> utf8(size + 1, 0);

    // 执行转换
    if (0 == WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        utf16.data(),
        static_cast<int>(utf16.size()),
        utf8.data(),
        size,
        nullptr,
        nullptr))
    {
        return "";
    }

    return utf8.data();
}

std::wstring Utf8ToUtf16(const std::string &utf8)
{
    if (utf8.empty())
    {
        return L"";
    }

    // 计算 UTF-16 所需字节数
    int size = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        utf8.data(),
        static_cast<int>(utf8.size()),
        nullptr,
        0);

    if (size <= 0)
    {
        return L"";
    }

    // 分配空间
    std::vector<wchar_t> utf16(size + 1, 0);

    // 执行转换
    if (0 == MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        utf8.data(),
        static_cast<int>(utf8.size()),
        utf16.data(),
        size))
    {
        return L"";
    }

    return utf16.data();
}

}
