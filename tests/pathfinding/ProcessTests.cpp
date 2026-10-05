#include "OwnedChildProcess.h"
#include "Support.h"
#include <territory/PathIO.h>
#include <thread>
#define NOMINMAX
#include <windows.h>
using namespace pf_test;
namespace {
bool until(const std::function<bool()> &f) {
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    if (f())
      return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  } while (std::chrono::steady_clock::now() < deadline);
  return false;
}
} // namespace
int process_tests(const std::filesystem::path &child) {
  TempDirectory temp;
  int n = 0;
  n += check("owned child passes literal Unicode/quotes/trailing slashes and "
             "captures log",
             [&] {
               OwnedChildProcess process;
               auto output = temp.path / L"ответ файл.json";
               const std::vector<std::wstring> values{
                   L"space path", L"quoted \"literal\"", L"trailing\\",
                   L"",           L"кириллица",          L"a&echo secret"};
               std::vector<std::wstring> args{L"--echo", output.wstring()};
               args.insert(args.end(), values.begin(), values.end());
               process.start(child, args, temp.path / "echo.log");
               if (!until([&] { return process.exit_code().has_value(); }) ||
                   process.exit_code() != 7 || !std::filesystem::exists(output))
                 return false;
               auto result = territory::read_json(output);
               if (result.size() != values.size())
                 return false;
               for (std::size_t i = 0; i < values.size(); ++i)
                 if (result[i] !=
                     territory::path_utf8(std::filesystem::path(values[i])))
                   return false;
               std::ifstream in(temp.path / "echo.log");
               std::string line;
               std::getline(in, line);
               return line == "child log marker";
             });
  n += check(
      "cancel stops owned grandchildren but leaves independent process alive",
      [&] {
        OwnedChildProcess tree, sentinel;
        tree.start(child,
                   {L"--tree", (temp.path / "grandchild.json").wstring()},
                   temp.path / "tree.log");
        sentinel.start(child,
                       {L"--hold", (temp.path / "sentinel.json").wstring()},
                       temp.path / "sentinel.log");
        if (!until([&] {
              return std::filesystem::exists(temp.path / "grandchild.json") &&
                     std::filesystem::exists(temp.path / "sentinel.json");
            }))
          return false;
        auto pid = territory::read_json(temp.path / "grandchild.json")
                       .at("pid")
                       .get<DWORD>();
        HANDLE grandchild = OpenProcess(SYNCHRONIZE, FALSE, pid);
        if (!grandchild)
          return false;
        tree.terminate_tree_owned();
        const bool stopped = until([&] {
          return tree.exit_code().has_value() &&
                 WaitForSingleObject(grandchild, 0) == WAIT_OBJECT_0;
        });
        CloseHandle(grandchild);
        const bool untouched = !sentinel.exit_code();
        sentinel.terminate_tree_owned();
        const bool cleaned =
            until([&] { return sentinel.exit_code().has_value(); });
        return stopped && untouched && cleaned;
      });
  n += check("missing executable fails explicitly without keeping owned tree",
             [&] {
               OwnedChildProcess process;
               return rejects([&] {
                 process.start(temp.path / "missing.exe", {},
                               temp.path / "missing.log");
               });
             });
  return n;
}
