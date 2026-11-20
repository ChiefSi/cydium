#pragma once

// Product name and version, for the About page and logs.
namespace buildinfo
{

// CYD_PRODUCT_NAME (default "cydium").
const char* name();

// CYD_VERSION_STRING, or "<git describe>[-dirty]-<build time>", with the
// build time in seconds since the Unix epoch.
const char* version();

}  // namespace buildinfo
