#include <exception>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <territory/JobRunner.h>
#include <territory/PathIO.h>
int job_tests();
int reference_tests();
int texture_tests();
int material_tests();
int geometry_tests();
int terrain_geometry_tests();
int geometric_normal_tests();
int raster_tests();
int output_tests();
int runner_tests();
int process_tests();
int cli_tests();
int ui_tests();
int gpu_raster_tests(const std::filesystem::path &);
int gpu_map_test(const std::filesystem::path &, const std::string &,
                 const std::filesystem::path &);
int scene_audit(const std::filesystem::path &, const std::string &,
                const std::filesystem::path &, bool);
int material_audit(const std::filesystem::path &, const std::string &,
                   const std::filesystem::path &);
int main(int argc, char **argv) {
  if (argc == 3 && std::string_view(argv[1]) == "--render-job")
    return territory::run_job_file(territory::path_from_utf8(argv[2]));
  if (argc == 5 && (std::string_view(argv[1]) == "--inspect-client" ||
                    std::string_view(argv[1]) == "--render-client")) {
    try {
      auto client = territory::path_from_utf8(argv[2]);
      auto dir = territory::create_job_directory(
          territory::path_from_utf8(argv[4]), client);
      territory::Job job{"runner-integration",
                         dir,
                         client,
                         {argv[3]},
                         std::string_view(argv[1]) == "--inspect-client"
                             ? territory::Mode::Inspect
                             : territory::Mode::Render,
                         {4096, true}};
      auto result =
          territory::run_job(job, {}, {}, territory::default_runner_services());
      std::cout << territory::result_json(job, result).dump() << '\n';
      return result.exit_code;
    } catch (const std::exception &e) {
      std::cerr << "Runner integration failed: " << e.what() << '\n';
      return 1;
    }
  }
  if (argc == 5 && std::string_view(argv[1]) == "--gpu-map") {
    try {
      return gpu_map_test(territory::path_from_utf8(argv[2]), argv[3],
                          territory::path_from_utf8(argv[4]));
    } catch (const std::exception &e) {
      std::cerr << "GPU map failed: " << e.what() << '\n';
      return 1;
    }
  }
  if (argc == 3 && std::string_view(argv[1]) == "--gpu-raster") {
    try {
      return gpu_raster_tests(territory::path_from_utf8(argv[2]));
    } catch (const std::exception &e) {
      std::cerr << "GPU test failed: " << e.what() << '\n';
      return 1;
    }
  }
  if (argc == 5 && (std::string_view(argv[1]) == "--audit-client" ||
                    std::string_view(argv[1]) == "--inventory-client")) {
    try {
      return scene_audit(territory::path_from_utf8(argv[2]), argv[3],
                         territory::path_from_utf8(argv[4]),
                         std::string_view(argv[1]) == "--inventory-client");
    } catch (const std::exception &e) {
      std::cerr << "Audit failed: " << e.what() << '\n';
      return 1;
    }
  }
  if (argc == 5 && std::string_view(argv[1]) == "--audit-materials") {
    try {
      return material_audit(territory::path_from_utf8(argv[2]), argv[3],
                            territory::path_from_utf8(argv[4]));
    } catch (const std::exception &e) {
      std::cerr << "Audit failed: " << e.what() << '\n';
      return 1;
    }
  }
  std::string_view suite = argc == 3 ? argv[2] : "";
  if (argc != 1 &&
      !(argc == 3 && std::string_view(argv[1]) == "--suite" &&
        (suite == "contract" || suite == "references" || suite == "texture" ||
         suite == "material" || suite == "geometry" || suite == "raster" ||
         suite == "output" || suite == "runner" || suite == "process" ||
         suite == "cli" || suite == "ui"))) {
    std::cerr << "Unknown territory test arguments\n";
    return 2;
  }
  try {
    int failures = 0;
    if (suite.empty() || suite == "contract")
      failures += job_tests();
    if (suite.empty() || suite == "references")
      failures += reference_tests();
    if (suite.empty() || suite == "texture")
      failures += texture_tests();
    if (suite.empty() || suite == "material")
      failures += material_tests();
    if (suite.empty() || suite == "geometry") {
      failures += geometry_tests();
      failures += terrain_geometry_tests();
      failures += geometric_normal_tests();
    }
    if (suite.empty() || suite == "raster")
      failures += raster_tests();
    if (suite.empty() || suite == "output")
      failures += output_tests();
    if (suite.empty() || suite == "runner")
      failures += runner_tests();
    if (suite.empty() || suite == "process")
      failures += process_tests();
    if (suite.empty() || suite == "cli")
      failures += cli_tests();
    if (suite.empty() || suite == "ui")
      failures += ui_tests();
    if (failures)
      std::cerr << failures << " test assertions failed\n";
    else
      std::cout << "Territory tests passed\n";
    return failures ? 1 : 0;
  } catch (const std::exception &e) {
    std::cerr << "Test exception: " << e.what() << '\n';
    return 1;
  }
}
