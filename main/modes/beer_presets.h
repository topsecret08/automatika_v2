#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BST_MASH = 0,
    BST_BOIL,
    BST_HOP,
    BST_CHILL,
    BST_XFER,
    BST_REST,
    BST_CUSTOM,
} beer_step_type_t;

typedef struct {
    uint8_t  type;
    char     name[32];
    float    temp;
    uint32_t duration_sec;
    bool     wait_for_temp;
    char     notes[64];
} beer_step_t;

const beer_step_t* beer_preset_get(int idx, int *out_count);
int  beer_preset_count(void);