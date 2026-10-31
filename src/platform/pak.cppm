module;
#include <algorithm>
#include <cstring>
#include <string>
#include <expected>
#include <utility>
#include <vector>

export module crumb.platform.pak;
import crumb.core.types;
import crumb.core.error;

export namespace crumb::pak {

using std::vector;

// "CRUMBPAK" image, "CRUMBEND" footer, and their fixed layout sizes.
inline constexpr char PAK_MAGIC[8]    = { 'C','R','U','M','B','P','A','K' };
inline constexpr char FOOTER_MAGIC[8] = { 'C','R','U','M','B','E','N','D' };
inline constexpr ui32 FORMAT_VERSION = 1;
inline constexpr usize HEADER_SIZE   = 16;  // magic + version + count
inline constexpr usize FOOTER_SIZE   = 24;  // magic + offset + size + crc + reserved

struct Entry {
  string name;
  ui32 offset = 0;
  ui32 size = 0;
  ui32 crc = 0;
};

struct SourceFile {
  string name;
  vector<ui8> bytes;
};

// Read-only archive backed by one contiguous byte buffer.
class Archive {
public:
  bool Parse(vector<ui8> bytes, string* error);
  bool Read(string_view name, vector<ui8>& out, string* error) const;
  bool Contains(string_view name) const;
  const vector<Entry>& Entries() const { return m_entries; }

private:
  vector<ui8> m_bytes;
  vector<Entry> m_entries;
};

// Standard IEEE CRC-32 (reflected, polynomial 0xEDB88320).
ui32 Crc32(const ui8* data, usize size);

// Build a pak image. Names are normalized to forward slashes and sorted so the
// output is reproducible.
Result<vector<ui8>> Build(vector<SourceFile> files);

// Append the footer used by "template exe + pak" single-file output.
void AppendFooter(vector<ui8>& file, ui32 pak_offset, ui32 pak_size, ui32 pak_crc);
bool ParseFooter(const vector<ui8>& file, ui32& pak_offset, ui32& pak_size,
                 string* error);

}

namespace crumb::pak {

namespace {
using namespace crumb;
void WriteU16(vector<ui8>& out, ui16 value) {
  out.push_back(to_ui8(value & 0xFF));
  out.push_back(to_ui8((value >> 8) & 0xFF));
}

void WriteU32(vector<ui8>& out, ui32 value) {
  out.push_back(to_ui8(value & 0xFF));
  out.push_back(to_ui8((value >> 8) & 0xFF));
  out.push_back(to_ui8((value >> 16) & 0xFF));
  out.push_back(to_ui8((value >> 24) & 0xFF));
}

ui16 ReadU16(const vector<ui8>& in, usize offset) {
  return to_ui16(in[offset]) |
         to_ui16(in[offset + 1] << 8);
}

ui32 ReadU32(const vector<ui8>& in, usize offset) {
  return to_ui32(in[offset]) |
         (to_ui32(in[offset + 1]) << 8) |
         (to_ui32(in[offset + 2]) << 16) |
         (to_ui32(in[offset + 3]) << 24);
}

// Pak names always use forward slashes and have no leading separator.
string NormalizeName(string_view name) {
  string result;
  result.reserve(name.size());
  for (char ch : name) {
    if (ch == '\\')
      result.push_back('/');
    else
      result.push_back(ch);
  }
  while (!result.empty() && result.front() == '/')
    result.erase(result.begin());
  return result;
}

// Entry names must fit the 16-bit length field and stay relative.
bool ValidName(string_view name) {
  if (name.empty() || name.size() > 0xFFFF)
    return false;
  if (name.front() == '/' || name.front() == '\\')
    return false;
  return name.find("..") == string_view::npos;
}

}

ui32 Crc32(const ui8* data, usize size) {
  ui32 crc = 0xFFFFFFFFu;
  for (usize i = 0; i < size; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      const ui32 mask = 0u - (crc & 1u);
      crc = (crc >> 1) ^ (0xEDB88320u & mask);
    }
  }
  return ~crc;
}

// Parse the header and entry table; every payload range is bound-checked.
bool Archive::Parse(vector<ui8> bytes, string* error) {
  if (bytes.size() < HEADER_SIZE) {
    if (error) *error = "pak is too small";
    return false;
  }
  if (std::memcmp(bytes.data(), PAK_MAGIC, sizeof(PAK_MAGIC)) != 0) {
    if (error) *error = "pak magic mismatch";
    return false;
  }

  const ui32 version = ReadU32(bytes, 8);
  if (version != FORMAT_VERSION) {
    if (error) *error = "unsupported pak version";
    return false;
  }

  const ui32 count = ReadU32(bytes, 12);
  usize cursor = HEADER_SIZE;
  vector<Entry> entries;
  entries.reserve(count);

  for (ui32 i = 0; i < count; ++i) {
    if (cursor + 2 > bytes.size()) {
      if (error) *error = "pak entry table truncated";
      return false;
    }
    const ui16 name_size = ReadU16(bytes, cursor);
    cursor += 2;
    if (cursor + name_size + 12 > bytes.size()) {
      if (error) *error = "pak entry truncated";
      return false;
    }

    Entry entry;
    entry.name.assign(
      reinterpret_cast<const char*>(bytes.data() + cursor), name_size);
    cursor += name_size;
    entry.offset = ReadU32(bytes, cursor); cursor += 4;
    entry.size   = ReadU32(bytes, cursor); cursor += 4;
    entry.crc    = ReadU32(bytes, cursor); cursor += 4;

    if (entry.offset > bytes.size() || entry.size > bytes.size() - entry.offset) {
      if (error) *error = "pak entry out of bounds: " + entry.name;
      return false;
    }
    entries.push_back(std::move(entry));
  }

  m_bytes = std::move(bytes);
  m_entries = std::move(entries);
  return true;
}

// Names are normalized before comparison; lookups are linear.
bool Archive::Contains(string_view name) const {
  const string normalized = NormalizeName(name);
  for (const Entry& entry : m_entries)
    if (entry.name == normalized)
      return true;
  return false;
}

// Copy one entry after verifying its per-entry CRC.
bool Archive::Read(string_view name, vector<ui8>& out, string* error) const {
  const string normalized = NormalizeName(name);
  for (const Entry& entry : m_entries) {
    if (entry.name != normalized)
      continue;
    const ui8* begin = m_bytes.data() + entry.offset;
    if (Crc32(begin, entry.size) != entry.crc) {
      if (error) *error = "pak crc mismatch: " + entry.name;
      return false;
    }
    out.assign(begin, begin + entry.size);
    return true;
  }
  if (error) *error = "pak entry not found: " + normalized;
  return false;
}

// Normalize/sort entries, write the table, then concatenate the payloads.
Result<vector<ui8>> Build(vector<SourceFile> files) {
  for (SourceFile& file : files) {
    file.name = NormalizeName(file.name);
    if (!ValidName(file.name))
      return std::unexpected<Error> {
        MakeError(ErrorCode::ERROR_INVALID_ARGUMENT,
                  "invalid pak entry name: " + file.name) };
    if (file.bytes.size() > 0xFFFFFFFFu)
      return std::unexpected<Error> {
        MakeError(ErrorCode::ERROR_INVALID_ARGUMENT,
                  "pak entry too large: " + file.name) };
  }
  std::sort(files.begin(), files.end(),
            [](const SourceFile& a, const SourceFile& b) {
              return a.name < b.name;
            });

  usize table_size = 0;
  for (const SourceFile& file : files)
    table_size += 2 + file.name.size() + 12;

  const usize payload_offset = HEADER_SIZE + table_size;
  if (payload_offset > 0xFFFFFFFFu)
    return std::unexpected<Error> {
      MakeError(ErrorCode::ERROR_OVERFLOW, "pak entry table too large") };

  vector<ui8> image;
  image.reserve(payload_offset + 1024);
  image.insert(image.end(), PAK_MAGIC, PAK_MAGIC + sizeof(PAK_MAGIC));
  WriteU32(image, FORMAT_VERSION);
  WriteU32(image, to_ui32(files.size()));

  ui32 offset = to_ui32(payload_offset);
  for (const SourceFile& file : files) {
    WriteU16(image, to_ui16(file.name.size()));
    image.insert(image.end(), file.name.begin(), file.name.end());
    WriteU32(image, offset);
    WriteU32(image, to_ui32(file.bytes.size()));
    WriteU32(image, Crc32(file.bytes.data(), file.bytes.size()));
    offset += to_ui32(file.bytes.size());
  }

  for (const SourceFile& file : files)
    image.insert(image.end(), file.bytes.begin(), file.bytes.end());

  return image;
}

// Layout: magic, offset, size, CRC, then a reserved zero word.
void AppendFooter(vector<ui8>& file, ui32 pak_offset, ui32 pak_size, ui32 pak_crc) {
  file.insert(file.end(), FOOTER_MAGIC, FOOTER_MAGIC + sizeof(FOOTER_MAGIC));
  WriteU32(file, pak_offset);
  WriteU32(file, pak_size);
  WriteU32(file, pak_crc);
  WriteU32(file, 0);
}

// Read offset/size from the trailing footer and validate the payload range.
// The stored CRC is not verified here.
bool ParseFooter(const vector<ui8>& file, ui32& pak_offset, ui32& pak_size,
                 string* error) {
  if (file.size() < FOOTER_SIZE) {
    if (error) *error = "file is too small for a crumb footer";
    return false;
  }
  const usize base = file.size() - FOOTER_SIZE;
  if (std::memcmp(file.data() + base, FOOTER_MAGIC, sizeof(FOOTER_MAGIC)) != 0) {
    if (error) *error = "crumb footer magic mismatch";
    return false;
  }
  pak_offset = ReadU32(file, base + 8);
  pak_size   = ReadU32(file, base + 12);
  if (pak_offset > file.size() || pak_size > file.size() - pak_offset) {
    if (error) *error = "crumb footer points outside the file";
    return false;
  }
  return true;
}

}
