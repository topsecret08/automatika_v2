#pragma once

#include "esp_attr.h"

// Крупные статические буферы, которым нечего делать во внутренней RAM.
// При включённом CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY линкер
// размещает их в PSRAM; в тестовой сборке без PSRAM — в обычном BSS.
#if defined(CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY) && \
    CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY
#define LARGE_BSS_ATTR EXT_RAM_BSS_ATTR
#else
#define LARGE_BSS_ATTR
#endif
