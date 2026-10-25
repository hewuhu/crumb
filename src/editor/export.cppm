module;
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

export module crumb.editor.exporter;
import crumb.core.types;
import crumb.core.error;
import crumb.core.log;
import crumb.platform.launch;
import crumb.platform.pak;

export namespace crumb {

using std::vector;

enum class ExportFormat : ui8 {
  SINGLE_FILE,
  EXE_PAK,
};

struct ExportOptions {
  string projectPath;
  // Empty means <project>/dist/<project>.exe.
  string outputPath;
  // Empty means the crumb_template_win64.exe next to the editor.
  string templatePath;
  ExportFormat format = ExportFormat::SINGLE_FILE;
};

// Copies the template, packs the project files, and writes either a single
// executable (template + pak + footer) or exe + pak. Returns the exe path.
Result<string> ExportProject(ExportOptions options);

} // namespace crumb

namespace crumb {

namespace {

constexpr c_string TEMPLATE_NAME = "crumb_template_win64.exe";
constexpr c_string MAIN_SCRIPT   = "main.lua";

bool ReadWholeFile(const std::filesystem::path& path, vector<ui8>& out,
                   string* error) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    if (error) *error = "cannot open " + path.string();
    return false;
  }
  out.assign(std::istreambuf_iterator<char>(file),
             std::istreambuf_iterator<char>());
  if (!file.good() && !file.eof()) {
    if (error) *error = "cannot read " + path.string();
    return false;
  }
  return true;
}

bool WriteWholeFile(const std::filesystem::path& path, const vector<ui8>& bytes,
                    string* error) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file) {
    if (error) *error = "cannot create " + path.string();
    return false;
  }
  file.write(reinterpret_cast<const char*>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  if (!file.good()) {
    if (error) *error = "cannot write " + path.string();
    return false;
  }
  return true;
}

bool SkipDirectory(string_view name) {
  return name == ".git" || name == "build" || name == "dist" ||
         name == "meta" || name == "templates" || name == ".vscode";
}

bool KeepFile(const std::filesystem::path& path) {
  const string extension = path.extension().string();
  return extension == ".lua" || extension == ".png" || extension == ".cfg";
}

Result<vector<pak::SourceFile>> CollectFiles(
  const std::filesystem::path& project_dir) {
  vector<pak::SourceFile> files;
  std::error_code ec;
  std::filesystem::recursive_directory_iterator it(
    project_dir, std::filesystem::directory_options::skip_permission_denied, ec);
  std::filesystem::recursive_directory_iterator end;
  if (ec) {
    return std::unexpected<Error> {
      MakeError(ErrorCode::ERROR_IO,
                "cannot walk " + project_dir.string() + ": " + ec.message()) };
  }

  for (; it != end; it.increment(ec)) {
    if (ec) {
      return std::unexpected<Error> {
        MakeError(ErrorCode::ERROR_IO, "cannot walk project: " + ec.message()) };
    }
    if (it->is_directory(ec)) {
      if (!ec && SkipDirectory(it->path().filename().string()))
        it.disable_recursion_pending();
      continue;
    }
    if (!it->is_regular_file(ec) || !KeepFile(it->path()))
      continue;

    string read_error;
    pak::SourceFile file;
    file.name = it->path().lexically_relative(project_dir).generic_string();
    if (!ReadWholeFile(it->path(), file.bytes, &read_error)) {
      return std::unexpected<Error> {
        MakeError(ErrorCode::ERROR_IO, read_error) };
    }
    files.push_back(std::move(file));
  }

  return files;
}

Result<std::filesystem::path> FindProjectDirectory(string_view project_path) {
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::path path = project_path.empty()
    ? fs::current_path(ec)
    : fs::path(string(project_path));
  if (ec) {
    return std::unexpected<Error> {
      MakeError(ErrorCode::ERROR_IO, "cannot resolve project: " + ec.message()) };
  }

  if (fs::is_regular_file(path, ec) && path.extension() == ".lua")
    path = path.parent_path();

  // A trailing separator makes filename() empty; drop it before using the
  // directory name as the default output name.
  while (path.filename().empty() && path.has_parent_path() &&
         path.parent_path() != path)
    path = path.parent_path();

  if (!fs::is_directory(path, ec)) {
    return std::unexpected<Error> {
      MakeError(ErrorCode::ERROR_NOT_FOUND,
                "project directory not found: " + path.string()) };
  }

  if (!fs::is_regular_file(path / MAIN_SCRIPT, ec)) {
    return std::unexpected<Error> {
      MakeError(ErrorCode::ERROR_NOT_FOUND,
                "main.lua not found in " + path.string()) };
  }

  return path;
}

Result<std::filesystem::path> FindTemplate(const ExportOptions& options) {
  namespace fs = std::filesystem;
  std::error_code ec;

  if (!options.templatePath.empty()) {
    const fs::path path{ string(options.templatePath) };
    if (fs::is_regular_file(path, ec))
      return path;
    return std::unexpected<Error> {
      MakeError(ErrorCode::ERROR_NOT_FOUND,
                "template not found: " + path.string()) };
  }

  const string dir = platform::MakeLaunchOptions().executableDir;
  const fs::path path = fs::path(dir) / TEMPLATE_NAME;
  if (fs::is_regular_file(path, ec))
    return path;

  return std::unexpected<Error> {
    MakeError(ErrorCode::ERROR_NOT_FOUND,
              "crumb_template_win64.exe not found next to the editor; "
              "build the template target or pass --template") };
}

string ProjectStem(const std::filesystem::path& project_dir) {
  std::filesystem::path path = project_dir;
  while (path.filename().empty() && path.has_parent_path() &&
         path.parent_path() != path)
    path = path.parent_path();
  const string name = path.filename().string();
  if (name.empty() || name == "." || name == "..")
    return "Game";
  return name;
}

Result<std::filesystem::path> MakeOutputPath(
  const std::filesystem::path& project_dir, string_view output_path) {
  namespace fs = std::filesystem;
  std::error_code ec;

  const string stem = ProjectStem(project_dir) + ".exe";
  fs::path output;
  if (output_path.empty()) {
    output = project_dir / "dist" / stem;
  } else {
    output = fs::path(string(output_path));
    if (fs::is_directory(output, ec))
      output /= stem;
    else if (!output.has_extension())
      output += ".exe";
  }

  if (output.has_parent_path())
    fs::create_directories(output.parent_path(), ec);
  if (ec) {
    return std::unexpected<Error> {
      MakeError(ErrorCode::ERROR_IO,
                "cannot create output directory: " + ec.message()) };
  }
  return output;
}

} // namespace

Result<string> ExportProject(ExportOptions options) {
  namespace fs = std::filesystem;

  const Result<fs::path> project = FindProjectDirectory(options.projectPath);
  if (!project)
    return std::unexpected<Error> { project.error() };

  const Result<fs::path> template_path = FindTemplate(options);
  if (!template_path)
    return std::unexpected<Error> { template_path.error() };

  const Result<vector<pak::SourceFile>> files = CollectFiles(*project);
  if (!files)
    return std::unexpected<Error> { files.error() };

  Result<vector<ui8>> pak_image = pak::Build(vector<pak::SourceFile>(*files));
  if (!pak_image)
    return std::unexpected<Error> { pak_image.error() };
  if (pak_image->size() > 0xFFFFFFFFu) {
    return std::unexpected<Error> {
      MakeError(ErrorCode::ERROR_OVERFLOW, "pak is larger than 4 GiB") };
  }

  vector<ui8> template_bytes;
  string read_error;
  if (!ReadWholeFile(*template_path, template_bytes, &read_error)) {
    return std::unexpected<Error> {
      MakeError(ErrorCode::ERROR_IO, read_error) };
  }
  if (template_bytes.size() > 0xFFFFFFFFu) {
    return std::unexpected<Error> {
      MakeError(ErrorCode::ERROR_OVERFLOW, "template is larger than 4 GiB") };
  }

  const Result<fs::path> output =
    MakeOutputPath(*project, options.outputPath);
  if (!output)
    return std::unexpected<Error> { output.error() };

  string write_error;
  if (options.format == ExportFormat::SINGLE_FILE) {
    vector<ui8> image = std::move(template_bytes);
    const ui32 pak_offset = static_cast<ui32>(image.size());
    const ui32 pak_size = static_cast<ui32>(pak_image->size());
    const ui32 pak_crc = pak::Crc32(pak_image->data(), pak_image->size());
    image.insert(image.end(), pak_image->begin(), pak_image->end());
    pak::AppendFooter(image, pak_offset, pak_size, pak_crc);

    if (!WriteWholeFile(*output, image, &write_error)) {
      return std::unexpected<Error> {
        MakeError(ErrorCode::ERROR_IO, write_error) };
    }
  } else {
    if (!WriteWholeFile(*output, template_bytes, &write_error)) {
      return std::unexpected<Error> {
        MakeError(ErrorCode::ERROR_IO, write_error) };
    }
    const fs::path pak_path =
      output->parent_path() / (output->stem().string() + ".pak");
    if (!WriteWholeFile(pak_path, *pak_image, &write_error)) {
      return std::unexpected<Error> {
        MakeError(ErrorCode::ERROR_IO, write_error) };
    }
  }

  Logger::Info("export: {} file(s), {} bytes pak -> {}",
               files->size(), pak_image->size(), output->string());
  return output->string();
}

} // namespace crumb
