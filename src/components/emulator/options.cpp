// Command-line options. IDF's linux main() ignores its arguments, so they are
// read in a static constructor, which glibc calls with argc and argv.

#include "options.h"

#include <arpa/inet.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace emulator
{
namespace
{

Options s_options;

void usage(FILE* out, const char* prog)
{
  std::fprintf(out,
               "Usage: %s [--addr ADDR] [--port PORT]\n"
               "Runs the cydium image and shows its screen at http://ADDR:PORT/.\n"
               "\n"
               "  -a, --addr ADDR  IPv4 address to listen on (default 127.0.0.1;\n"
               "                   0.0.0.0 for all interfaces). Env: CYD_EMULATOR_ADDR\n"
               "  -p, --port PORT  port to listen on (default 8240). Env: CYD_EMULATOR_PORT\n"
               "  -h, --help       show this help\n"
               "\n"
               "Settings are kept in $CYD_EMULATOR_FLASH, else\n"
               "$XDG_STATE_HOME/cydium/emulator-flash.bin or\n"
               "~/.local/state/cydium/emulator-flash.bin.\n",
               prog);
}

[[noreturn]] void fail(const char* prog, const char* fmt, const char* arg)
{
  std::fprintf(stderr, "%s: ", prog);
  std::fprintf(stderr, fmt, arg);
  std::fprintf(stderr, "\nTry '%s --help'.\n", prog);
  std::exit(2);
}

void set_addr(const char* prog, const char* value)
{
  in_addr a;
  if (inet_pton(AF_INET, value, &a) != 1)
    fail(prog, "invalid address '%s' (use an IPv4 address such as 127.0.0.1)", value);
  s_options.addr = value;
}

void set_port(const char* prog, const char* value)
{
  char* end = nullptr;
  const long port = std::strtol(value, &end, 10);
  if (!*value || *end || port < 1 || port > 65535)
    fail(prog, "invalid port '%s' (use 1-65535)", value);
  s_options.port = static_cast<int>(port);
}

// Matches "-s VALUE", "--long VALUE" or "--long=VALUE"; advances i past it.
const char* value_of(int argc, char** argv, int& i, const char* s, const char* l)
{
  const char* arg = argv[i];
  const size_t n = std::strlen(l);
  if (std::strncmp(arg, l, n) == 0 && arg[n] == '=')
    return arg + n + 1;
  if (std::strcmp(arg, s) != 0 && std::strcmp(arg, l) != 0)
    return nullptr;
  if (i + 1 >= argc)
    fail(argv[0], "%s needs a value", arg);
  return argv[++i];
}

// Runs before flash.cpp sets up the flash file, so --help and errors exit
// first.
__attribute__((constructor(101))) void parse(int argc, char** argv, char**)
{
  const char* prog = argc > 0 && argv[0] ? argv[0] : "cydium-emulator";
  if (const char* env = std::getenv("CYD_EMULATOR_ADDR"); env && *env)
    set_addr(prog, env);
  if (const char* env = std::getenv("CYD_EMULATOR_PORT"); env && *env)
    set_port(prog, env);

  for (int i = 1; i < argc; ++i)
  {
    if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0)
    {
      usage(stdout, prog);
      std::exit(0);
    }
    if (const char* v = value_of(argc, argv, i, "-a", "--addr"))
      set_addr(prog, v);
    else if (const char* v = value_of(argc, argv, i, "-p", "--port"))
      set_port(prog, v);
    else
      fail(prog, "unknown option '%s'", argv[i]);
  }
}

}  // namespace

const Options& options()
{
  return s_options;
}

}  // namespace emulator
