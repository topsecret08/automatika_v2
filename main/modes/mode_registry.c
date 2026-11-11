#include "mode_base.h"

const process_mode_t* mode_from_type(process_type_t t) {
    switch (t) {
        case PROCESS_DISTILL: return &MODE_DISTILL;
        case PROCESS_RECTIFY: return &MODE_RECTIFY;
        case PROCESS_BEER:    return &MODE_BEER;
        case PROCESS_PVK:     return &MODE_PVK;
        case PROCESS_NBK:     return &MODE_NBK;
        case PROCESS_VACUUM:  return &MODE_VACUUM;
        case PROCESS_DOUBLE:
        case PROCESS_GABRIEL: return &MODE_DOUBLE;
        case PROCESS_FERMENT: return &MODE_FERMENT;
        default: return NULL;
    }
}