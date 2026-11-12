#ifndef IMGUI_APP_UTILS_H_
#define IMGUI_APP_UTILS_H_

#include "imgui.h"
#include "lccl/oss/fmt.h"

namespace app
{

template<typename... Args>
inline void ImGuiTextFmt(fmt::format_string<Args...> fmt, Args &&... args)
{
    std::string content = fmt::vformat(fmt, fmt::make_format_args(args...));
    ImGui::Text("%s", content.c_str());
}

}

#endif // !IMGUI_APP_UTILS_H_
