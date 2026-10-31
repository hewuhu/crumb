module;
#include <filesystem>
#include <fstream>
#include <iterator>
#include <expected>
#include <system_error>
#include <utility>
#include <vector>

export module crumb.platform.vfs;
import crumb.core.types;
import crumb.core.error;
import crumb.platform.pak;

export namespace crumb::vfs {

using std::vector;

// How the current project is stored.
enum class SourceKind : ui8 {
  DISK,
  PAK,
  SELF,
};

// Read-only view over a project directory, a .pak file, or an executable with
// an appended pak + footer.
class Vfs {
public:
  bool MountDisk(string_view directory, string* error = nullptr);
  bool MountPakFile(string_view pak_path, string* error = nullptr);
  bool MountSelf(string_view exe_path, string* error = nullptr);

  // Directory / .pak / .exe / .lua. A .exe falls back from footer to sibling
  // pak to its own directory (dev template).
  bool MountAuto(string_view path, string* error = nullptr);
  void Unmount();

  bool IsMounted() const { return m_mounted; }
  SourceKind Kind() const { return m_kind; }
  const string& SourcePath() const { return m_root; }

  bool Exists(string_view name) const;
  Result<vector<ui8>> ReadBinary(string_view name) const;
  Result<string> ReadText(string_view name) const;

private:
  bool MountBytes(vector<ui8> bytes, SourceKind kind, string source,
                  string* error);

  SourceKind m_kind = SourceKind::DISK;
  string m_root;
  pak::Archive m_archive;
  bool m_mounted = false;
};

}

namespace crumb::vfs {

namespace {

// Read a file into a byte vector; the stream is always closed by RAII.
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

// Basic path guard: non-empty, no leading separator, and no ".." substring.
bool ValidName(string_view name) {
  if (name.empty() || name.front() == '/' || name.front() == '\\')
    return false;
  return name.find("..") == string_view::npos;
}

// Join a validated logical name onto the disk root.
std::filesystem::path Join(const string& root, string_view name) {
  return std::filesystem::path(root) / std::filesystem::path(string(name));
}

}

// Mount a project directory; names are resolved relative to it.
bool Vfs::MountDisk(string_view directory, string* error) {
  std::error_code ec;
  const std::filesystem::path path{ string(directory) };
  if (!std::filesystem::is_directory(path, ec)) {
    if (error) *error = "not a directory: " + path.string();
    return false;
  }
  m_kind = SourceKind::DISK;
  m_root = path.string();
  m_archive = {};
  m_mounted = true;
  return true;
}

// Read and parse a standalone .pak file.
bool Vfs::MountPakFile(string_view pak_path, string* error) {
  vector<ui8> bytes;
  if (!ReadWholeFile(std::filesystem::path(string(pak_path)), bytes, error))
    return false;
  return MountBytes(std::move(bytes), SourceKind::PAK, string(pak_path), error);
}

// Read the executable, cut out the pak described by its footer, and parse it.
bool Vfs::MountSelf(string_view exe_path, string* error) {
  vector<ui8> bytes;
  if (!ReadWholeFile(std::filesystem::path(string(exe_path)), bytes, error))
    return false;

  ui32 offset = 0;
  ui32 size = 0;
  string footer_error;
  if (!pak::ParseFooter(bytes, offset, size, &footer_error)) {
    if (error) *error = footer_error;
    return false;
  }

  vector<ui8> pak_bytes(
    bytes.begin() + offset, bytes.begin() + offset + size);
  return MountBytes(std::move(pak_bytes), SourceKind::SELF, string(exe_path), error);
}

// Parse an archive and only commit it to this Vfs on success.
bool Vfs::MountBytes(vector<ui8> bytes, SourceKind kind, string source,
                     string* error) {
  pak::Archive archive;
  string parse_error;
  if (!archive.Parse(std::move(bytes), &parse_error)) {
    if (error) *error = parse_error;
    return false;
  }
  m_kind = kind;
  m_root = std::move(source);
  m_archive = std::move(archive);
  m_mounted = true;
  return true;
}

bool Vfs::MountAuto(string_view path, string* error) {
  const std::filesystem::path target{ string(path) };
  std::error_code ec;

  if (std::filesystem::is_directory(target, ec))
    return MountDisk(target.string(), error);

  if (std::filesystem::is_regular_file(target, ec)) {
    const string extension = target.extension().string();
    if (extension == ".pak")
      return MountPakFile(target.string(), error);
    if (extension == ".lua")
      return MountDisk(target.parent_path().string(), error);
    if (extension == ".exe") {
      if (MountSelf(target.string(), nullptr))
        return true;

      const std::filesystem::path sibling =
        target.parent_path() / (target.stem().string() + ".pak");
      if (std::filesystem::is_regular_file(sibling, ec))
        return MountPakFile(sibling.string(), error);

      return MountDisk(target.parent_path().string(), error);
    }
  }

  if (error) *error = "cannot mount project path: " + string(path);
  return false;
}

// Drop the archive and return to the default DISK state.
void Vfs::Unmount() {
  m_kind = SourceKind::DISK;
  m_root.clear();
  m_archive = {};
  m_mounted = false;
}

// Existence check; invalid names are treated as missing.
bool Vfs::Exists(string_view name) const {
  if (!m_mounted || !ValidName(name))
    return false;
  if (m_kind == SourceKind::DISK) {
    std::error_code ec;
    return std::filesystem::is_regular_file(Join(m_root, name), ec);
  }
  return m_archive.Contains(name);
}

// Read a whole logical file; all failures are reported as Error values.
Result<vector<ui8>> Vfs::ReadBinary(string_view name) const {
  if (!m_mounted) {
    return std::unexpected<Error> {
      MakeError(ErrorCode::ERROR_IO, "vfs is not mounted") };
  }
  if (!ValidName(name)) {
    return std::unexpected<Error> {
      MakeError(ErrorCode::ERROR_INVALID_ARGUMENT,
                "invalid vfs path: " + string(name)) };
  }

  vector<ui8> bytes;
  if (m_kind == SourceKind::DISK) {
    string error;
    if (!ReadWholeFile(Join(m_root, name), bytes, &error)) {
      return std::unexpected<Error> {
        MakeError(ErrorCode::ERROR_NOT_FOUND, error) };
    }
    return bytes;
  }

  string error;
  if (!m_archive.Read(name, bytes, &error)) {
    return std::unexpected<Error> {
      MakeError(ErrorCode::ERROR_NOT_FOUND, error) };
  }
  return bytes;
}

// Byte-for-byte text read; BOM removal is left to the caller.
Result<string> Vfs::ReadText(string_view name) const {
  const Result<vector<ui8>> bytes = ReadBinary(name);
  if (!bytes)
    return std::unexpected<Error> { bytes.error() };
  return string(reinterpret_cast<const char*>(bytes->data()), bytes->size());
}

}
