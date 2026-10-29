// Single place that includes secrets.h, so a missing value fails the build
// with a clear message instead of an obscure compiler error.
#pragma once

#include "secrets.h"

#if !defined(WIFI_SSID) || !defined(WIFI_PASSWORD) || !defined(API_BASE_URL) || !defined(API_TOKEN)
#error "include/secrets.h is incomplete: copy include/secrets.h.example to include/secrets.h and fill in every value"
#endif
