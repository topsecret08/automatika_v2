#pragma once
typedef struct {
    const char *name;
    float target_temp;
    int   mode;    // 0=вода, 1=пар, 2=авто
} pvk_preset_t;

const pvk_preset_t* pvk_preset_get(int idx);
int pvk_preset_count(void);