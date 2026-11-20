#pragma once

#include <string>

namespace emulator
{

// Web server settings, from the command line (see options.cpp), else the
// CYD_EMULATOR_ADDR / CYD_EMULATOR_PORT environment variables, else defaults.
struct Options
{
  std::string addr = "127.0.0.1";
  int port = 8240;
};

const Options& options();

}  // namespace emulator
