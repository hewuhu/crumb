module;
#include <string>
#include <string_view>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <cstdio>
#endif

export module crumb.platform.dialog;
import crumb.core.types;

export namespace crumb::platform {

// Last-resort error reporting for GUI builds where stderr is not visible.
void ShowError(string_view title, string_view message);

}

#ifdef _WIN32

namespace {

// Convert UTF-8 to UTF-16 for the Win32 wide-character API.
std::wstring ToWide(crumb::string_view text) {
  if (text.empty())
    return {};
  const crumb::i32 size = ::MultiByteToWideChar(
    CP_UTF8, 0, text.data(), crumb::to_i32(text.size()), nullptr, 0);
  if (size <= 0)
    return {};
  std::wstring result(static_cast<crumb::usize>(size), L'\0');
  ::MultiByteToWideChar(
    CP_UTF8, 0, text.data(), crumb::to_i32(text.size()),
    result.data(), size);
  return result;
}

}

namespace crumb::platform {

void ShowError(string_view title, string_view message) {
  const std::wstring wide_title = ToWide(title);
  const std::wstring wide_message = ToWide(message);
  ::MessageBoxW(nullptr, wide_message.c_str(), wide_title.c_str(),
                MB_OK | MB_ICONERROR);
}

}

#else

namespace crumb::platform {

void ShowError(string_view title, string_view message) {
  std::fprintf(stderr, "%.*s: %.*s\n",
               crumb::to_i32(title.size()), title.data(),
               crumb::to_i32(message.size()), message.data());
}

}

#endif
