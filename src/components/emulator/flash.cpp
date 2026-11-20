// Keeps the emulated flash in a file, so settings saved in NVS survive a
// restart. Runs before app_main, as the partition layer maps the file on the
// first flash access.

#include <sys/stat.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "esp_private/partition_linux.h"
#include "sdkconfig.h"

extern const uint8_t kTableStart[] asm("_binary_partition_table_bin_start");
extern const uint8_t kTableEnd[] asm("_binary_partition_table_bin_end");

namespace emulator
{
namespace
{

#ifdef CONFIG_PARTITION_TABLE_OFFSET
constexpr size_t kTableOffset = CONFIG_PARTITION_TABLE_OFFSET;
#else
constexpr size_t kTableOffset = 0x8000;
#endif

// $CYD_EMULATOR_FLASH, else $XDG_STATE_HOME/cydium/emulator-flash.bin, else
// ~/.local/state/cydium/emulator-flash.bin.
std::string flash_path()
{
  const char* env = std::getenv("CYD_EMULATOR_FLASH");
  if (env && *env)
    return env;
  std::string dir;
  if ((env = std::getenv("XDG_STATE_HOME")) && *env)
    dir = env;
  else if ((env = std::getenv("HOME")) && *env)
    dir = std::string(env) + "/.local/state";
  else
    dir = ".";
  return dir + "/cydium/emulator-flash.bin";
}

void make_parent_dirs(const std::string& path)
{
  for (size_t i = path.find('/', 1); i != std::string::npos; i = path.find('/', i + 1))
    mkdir(path.substr(0, i).c_str(), 0755);  // errors show up when the file is created
}

// End of the last partition in the table: the flash size needed.
size_t flash_size()
{
  size_t size = kTableOffset + 0x1000;
  for (const uint8_t* e = kTableStart; e + 32 <= kTableEnd; e += 32)
  {
    if (e[0] != 0xAA || e[1] != 0x50)  // entry magic 0x50AA, little-endian
      break;
    auto u32 = [](const uint8_t* p)
    {
      return static_cast<size_t>(p[0] | p[1] << 8 | p[2] << 16 | static_cast<uint32_t>(p[3]) << 24);
    };
    const size_t end = u32(e + 4) + u32(e + 8);
    size = end > size ? end : size;
  }
  return size;
}

// Writes an erased flash image holding the partition table.
bool create_flash(const std::string& path)
{
  make_parent_dirs(path);
  std::vector<uint8_t> image(flash_size(), 0xFF);
  std::memcpy(image.data() + kTableOffset, kTableStart,
              static_cast<size_t>(kTableEnd - kTableStart));
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f)
    return false;
  const bool ok = std::fwrite(image.data(), 1, image.size(), f) == image.size();
  return std::fclose(f) == 0 && ok;
}

__attribute__((constructor(102))) void setup_flash()
{
  const std::string path = flash_path();
  struct stat st;
  if (stat(path.c_str(), &st) != 0 && !create_flash(path))
  {
    std::fprintf(stderr, "emulator: can't create %s (%s); settings won't be saved\n", path.c_str(),
                 std::strerror(errno));
    return;  // the partition layer falls back to a temporary file
  }
  std::fprintf(stderr, "emulator: flash in %s\n", path.c_str());
  esp_partition_file_mmap_ctrl_t* ctrl = esp_partition_get_file_mmap_ctrl_input();
  std::snprintf(ctrl->flash_file_name, sizeof(ctrl->flash_file_name), "%s", path.c_str());
  ctrl->flash_file_size = 0;
  ctrl->remove_dump = false;
}

}  // namespace
}  // namespace emulator
