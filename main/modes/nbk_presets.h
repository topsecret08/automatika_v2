#pragma once
typedef struct {
    const char *name;
    float cube_temp;
    float feed_lph;
} nbk_preset_t;

const nbk_preset_t* nbk_preset_get(int idx);
int nbk_preset_count(void);