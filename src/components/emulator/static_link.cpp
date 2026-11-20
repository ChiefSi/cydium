// The emulator is linked statically (see CMakeLists.txt), where
// dlsym(RTLD_NEXT, ...) finds nothing. IDF's linux port uses it to reach the
// C library's versions of two functions it hooks, so the link wraps dlsym
// (-Wl,--wrap=dlsym) and this hands those two over directly.

#include <dlfcn.h>
#include <sys/select.h>
#include <unistd.h>

#include <cstring>

namespace
{

// The C library's select, which FreeRTOS's select wrapper replaces.
int libc_select(int n, fd_set* r, fd_set* w, fd_set* e, struct timeval* tv)
{
  if (!tv)
    return pselect(n, r, w, e, nullptr, nullptr);
  const timespec ts{tv->tv_sec, tv->tv_usec * 1000};
  return pselect(n, r, w, e, &ts, nullptr);
}

}  // namespace

extern "C" void* __wrap_dlsym(void* handle, const char* name)
{
  if (handle != RTLD_NEXT || !name)
    return nullptr;
  if (std::strcmp(name, "ftruncate") == 0)  // esp_partition's flash file
    return reinterpret_cast<void*>(&ftruncate);
  if (std::strcmp(name, "select") == 0)  // freertos/esp_additions
    return reinterpret_cast<void*>(&libc_select);
  return nullptr;
}
