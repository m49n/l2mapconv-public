#include "ClientFolderPicker.h"

#include <utils/Log.h>

#include <iostream>
#include <memory>
#include <string>

#ifdef _WIN32
#include <ShObjIdl.h>
#include <Windows.h>

namespace {

class ComApartment {
public:
  ComApartment()
      : m_result(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)),
        m_uninitialize(SUCCEEDED(m_result)) {}

  ~ComApartment() {
    if (m_uninitialize) {
      CoUninitialize();
    }
  }

  auto ready() const -> bool {
    return SUCCEEDED(m_result) || m_result == RPC_E_CHANGED_MODE;
  }

  auto result() const -> HRESULT { return m_result; }

private:
  HRESULT m_result;
  bool m_uninitialize;
};

template <typename T> class ComPointer {
public:
  explicit ComPointer(T *pointer = nullptr) : m_pointer(pointer) {}
  ~ComPointer() {
    if (m_pointer != nullptr) {
      m_pointer->Release();
    }
  }

  ComPointer(const ComPointer &) = delete;
  auto operator=(const ComPointer &) -> ComPointer & = delete;

  auto operator->() const -> T * { return m_pointer; }
  explicit operator bool() const { return m_pointer != nullptr; }

private:
  T *m_pointer;
};

void log_failure(std::string_view operation, HRESULT result) {
  utils::Log(utils::LOG_ERROR, "App")
      << operation << " failed with HRESULT "
      << static_cast<unsigned long>(result) << std::endl;
}

auto utf8_to_wide(std::string_view value) -> std::wstring {
  if (value.empty()) {
    return {};
  }
  const auto size =
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                          static_cast<int>(value.size()), nullptr, 0);
  if (size <= 0) {
    return L"Unable to use the selected Lineage II client directory.";
  }
  std::wstring result(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                      static_cast<int>(value.size()), result.data(), size);
  return result;
}

} // namespace
#endif

auto choose_client_root_folder() -> std::optional<std::filesystem::path> {
#ifdef _WIN32
  const ComApartment apartment;
  if (!apartment.ready()) {
    log_failure("CoInitializeEx", apartment.result());
    return std::nullopt;
  }

  IFileOpenDialog *raw_dialog = nullptr;
  auto result =
      CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                       IID_PPV_ARGS(&raw_dialog));
  const ComPointer<IFileOpenDialog> dialog{raw_dialog};
  if (FAILED(result) || !dialog) {
    log_failure("CoCreateInstance(CLSID_FileOpenDialog)", result);
    return std::nullopt;
  }

  FILEOPENDIALOGOPTIONS options{};
  result = dialog->GetOptions(&options);
  if (FAILED(result)) {
    log_failure("IFileDialog::GetOptions", result);
    return std::nullopt;
  }
  result = dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
  if (FAILED(result)) {
    log_failure("IFileDialog::SetOptions", result);
    return std::nullopt;
  }
  result = dialog->SetTitle(L"Choose the Lineage II client sam directory");
  if (FAILED(result)) {
    log_failure("IFileDialog::SetTitle", result);
    return std::nullopt;
  }

  result = dialog->Show(nullptr);
  if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
    return std::nullopt;
  }
  if (FAILED(result)) {
    log_failure("IFileDialog::Show", result);
    return std::nullopt;
  }

  IShellItem *raw_item = nullptr;
  result = dialog->GetResult(&raw_item);
  const ComPointer<IShellItem> item{raw_item};
  if (FAILED(result) || !item) {
    log_failure("IFileDialog::GetResult", result);
    return std::nullopt;
  }

  PWSTR raw_path = nullptr;
  result = item->GetDisplayName(SIGDN_FILESYSPATH, &raw_path);
  if (FAILED(result) || raw_path == nullptr) {
    log_failure("IShellItem::GetDisplayName", result);
    return std::nullopt;
  }

  const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> selected_path{
      raw_path, &CoTaskMemFree};
  return std::filesystem::path{selected_path.get()};
#else
  return std::nullopt;
#endif
}

void show_client_selection_error(std::string_view message) {
#ifdef _WIN32
  const auto wide_message = utf8_to_wide(message);
  MessageBoxW(nullptr, wide_message.c_str(), L"l2mapconv",
              MB_OK | MB_ICONERROR | MB_TASKMODAL);
#else
  utils::Log(utils::LOG_ERROR, "App") << message << std::endl;
#endif
}
