#ifndef IMGUI_APP_UTILS_H_
#define IMGUI_APP_UTILS_H_

#include "imgui.h"
#include "lccl/oss/fmt.h"

namespace app
{

std::string Utf16ToUtf8(const std::wstring &utf16);
std::wstring Utf8ToUtf16(const std::string &utf8);

template<typename... Args>
inline void ImGuiTextFmt(fmt::format_string<Args...> fmt, Args &&... args)
{
    std::string content = fmt::vformat(fmt, fmt::make_format_args(args...));
    ImGui::Text("%s", content.c_str());
}

template<typename... Args>
inline void ImGuiInputTextFmt(const std::string &label, int flags, fmt::format_string<Args...> fmt, Args &&... args)
{
    std::vector<char> buffer;
    fmt::vformat_to(std::back_inserter(buffer), fmt, fmt::make_format_args(args...));
    buffer.push_back(0);
    ImGui::InputText(label.c_str(), buffer.data(), buffer.size(), flags);
}

}

#endif // !IMGUI_APP_UTILS_H_
