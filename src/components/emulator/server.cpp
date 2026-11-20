// Minimal HTTP server for the emulator page. It runs on plain threads with
// every signal blocked, so the FreeRTOS POSIX port's tick signal is always
// delivered to a firmware task, never to these.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#include "emulator.h"
#include "esp_log.h"
#include "options.h"
#include "state.h"

extern const char kPageStart[] asm("_binary_page_html_start");
extern const char kPageEnd[] asm("_binary_page_html_end");

namespace emulator
{
namespace
{

constexpr const char* kTag = "emulator";
constexpr int kFrameHeader = 12;
constexpr auto kLongPoll = std::chrono::milliseconds(1000);

void send_all(int fd, const void* data, size_t len)
{
  const char* p = static_cast<const char*>(data);
  while (len > 0)
  {
    const ssize_t n = send(fd, p, len, MSG_NOSIGNAL);
    if (n <= 0)
      return;
    p += n;
    len -= static_cast<size_t>(n);
  }
}

void reply(int fd, const char* status, const char* type, const void* body, size_t len)
{
  char head[256];
  const int n = std::snprintf(head, sizeof(head),
                              "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
                              "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
                              status, type, len);
  send_all(fd, head, static_cast<size_t>(n));
  if (len > 0)
    send_all(fd, body, len);
}

// Value of `key` in a query string like "a=1&b=2", or fallback.
int query_int(const char* query, const char* key, int fallback)
{
  const size_t key_len = std::strlen(key);
  for (const char* p = query; p && *p;)
  {
    if (std::strncmp(p, key, key_len) == 0 && p[key_len] == '=')
      return std::atoi(p + key_len + 1);
    p = std::strchr(p, '&');
    if (p)
      ++p;
  }
  return fallback;
}

void send_frame(int fd, uint32_t since)
{
  State& s = state();
  std::vector<uint8_t> body;
  {
    std::unique_lock<std::mutex> lock(s.mutex);
    s.changed.wait_for(lock, kLongPoll, [&] { return s.seq != since; });
    const size_t pixels = static_cast<size_t>(s.width) * s.height;
    body.resize(kFrameHeader + pixels * 2);
    uint8_t* h = body.data();
    // Little-endian header; pixels stay big-endian RGB565 as the panel gets them.
    h[0] = s.seq & 0xFF;
    h[1] = (s.seq >> 8) & 0xFF;
    h[2] = (s.seq >> 16) & 0xFF;
    h[3] = (s.seq >> 24) & 0xFF;
    h[4] = s.width & 0xFF;
    h[5] = s.width >> 8;
    h[6] = s.height & 0xFF;
    h[7] = s.height >> 8;
    h[8] = s.invert ? 1 : 0;
    h[9] = s.backlight;
    h[10] = static_cast<uint8_t>(s.orientation);
    h[11] = 0;
    std::memcpy(h + kFrameHeader, s.pixels, pixels * 2);
  }
  reply(fd, "200 OK", "application/octet-stream", body.data(), body.size());
}

void set_touch(const char* query)
{
  State& s = state();
  std::lock_guard<std::mutex> lock(s.mutex);
  const bool down = query_int(query, "down", 0) != 0;
  if (down && !s.pen_down)
  {
    s.pen_down_at_ms = now_ms();
    s.pen_orientation = s.orientation;  // what the user clicked on
  }
  s.pen_down = down;
  s.pen_x = query_int(query, "x", s.pen_x);
  s.pen_y = query_int(query, "y", s.pen_y);
}

void set_button(const char* query)
{
  State& s = state();
  std::lock_guard<std::mutex> lock(s.mutex);
  const bool down = query_int(query, "down", 0) != 0;
  if (down && !s.button_down)
    s.button_down_at_ms = now_ms();
  s.button_down = down;
}

void handle(int fd)
{
  char req[4096];
  size_t len = 0;
  while (len < sizeof(req) - 1)
  {
    const ssize_t n = recv(fd, req + len, sizeof(req) - 1 - len, 0);
    if (n <= 0)
      break;
    len += static_cast<size_t>(n);
    req[len] = '\0';
    if (std::strstr(req, "\r\n\r\n"))
      break;
  }
  req[len] = '\0';

  // "METHOD /path?query HTTP/1.1"
  char* path = std::strchr(req, ' ');
  char* end = path ? std::strchr(path + 1, ' ') : nullptr;
  if (!path || !end)
  {
    reply(fd, "400 Bad Request", "text/plain", "", 0);
    close(fd);
    return;
  }
  ++path;
  *end = '\0';
  char* query = std::strchr(path, '?');
  if (query)
    *query++ = '\0';

  if (std::strcmp(path, "/") == 0)
    reply(fd, "200 OK", "text/html; charset=utf-8", kPageStart,
          static_cast<size_t>(kPageEnd - kPageStart));
  else if (std::strcmp(path, "/frame") == 0)
    send_frame(fd, static_cast<uint32_t>(query_int(query, "since", 0)));
  else if (std::strcmp(path, "/touch") == 0)
  {
    set_touch(query);
    reply(fd, "204 No Content", "text/plain", "", 0);
  }
  else if (std::strcmp(path, "/button") == 0)
  {
    set_button(query);
    reply(fd, "204 No Content", "text/plain", "", 0);
  }
  else
    reply(fd, "404 Not Found", "text/plain", "not found", 9);
  close(fd);
}

void* serve(void* arg)
{
  const int listen_fd = static_cast<int>(reinterpret_cast<intptr_t>(arg));
  for (;;)
  {
    const int fd = accept(listen_fd, nullptr, nullptr);
    if (fd >= 0)
      std::thread(handle, fd).detach();  // inherits the blocked signal mask
  }
  return nullptr;
}

}  // namespace

void start()
{
  static std::once_flag once;
  std::call_once(once,
                 []
                 {
                   const char* addr = options().addr.c_str();
                   const int port = options().port;

                   sockaddr_in sa{};
                   sa.sin_family = AF_INET;
                   sa.sin_port = htons(static_cast<uint16_t>(port));
                   inet_pton(AF_INET, addr, &sa.sin_addr);  // checked by options.cpp
                   const int fd = socket(AF_INET, SOCK_STREAM, 0);
                   const int one = 1;
                   setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
                   if (fd < 0 || bind(fd, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) != 0 ||
                       listen(fd, 16) != 0)
                   {
                     ESP_LOGE(kTag, "can't listen on %s:%d: %s", addr, port, std::strerror(errno));
                     if (fd >= 0)
                       close(fd);
                     return;
                   }

                   sigset_t all, old;
                   sigfillset(&all);
                   pthread_sigmask(SIG_SETMASK, &all, &old);
                   pthread_t thread;
                   const int err = pthread_create(
                       &thread, nullptr, serve, reinterpret_cast<void*>(static_cast<intptr_t>(fd)));
                   pthread_sigmask(SIG_SETMASK, &old, nullptr);
                   if (err != 0)
                   {
                     ESP_LOGE(kTag, "can't start the server thread: %s", std::strerror(err));
                     close(fd);
                     return;
                   }
                   pthread_detach(thread);
                   ESP_LOGI(kTag, "screen at http://%s:%d/", addr, port);
                 });
}

}  // namespace emulator
