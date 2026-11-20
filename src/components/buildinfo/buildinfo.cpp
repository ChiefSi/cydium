#include "buildinfo.h"

#include "buildinfo_gen.h"
#include "sdkconfig.h"

namespace buildinfo
{

const char* name()
{
  return CONFIG_CYD_PRODUCT_NAME;
}

const char* version()
{
#ifdef CONFIG_CYD_VERSION_CUSTOM
  return CONFIG_CYD_VERSION_STRING;
#else
  return CYD_BUILDINFO_GENERATED_VERSION;
#endif
}

}  // namespace buildinfo
