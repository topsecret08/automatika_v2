#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "beer_presets.h"

#define BEER_MAX_STEPS  32
#define BEER_MAX_RECIPES 20

typedef struct {
    char        name[48];
    uint8_t     step_count;
    beer_step_t steps[BEER_MAX_STEPS];
    float       batch_size_l;
    float       og, fg;
    float       ibu, srm;
    uint32_t    created_ms;
} beer_recipe_t;

void  beer_recipe_init(void);
int   beer_recipe_list(beer_recipe_t **out);
beer_recipe_t* beer_recipe_new(void);
beer_recipe_t* beer_recipe_get(int idx);
bool  beer_recipe_save(const beer_recipe_t *r);
bool  beer_recipe_delete(int idx);
bool  beer_recipe_export(const beer_recipe_t *r, const char *path);
bool  beer_recipe_import(const char *path, beer_recipe_t *out);

// Runtime
void  beer_recipe_start(int idx);
void  beer_recipe_next_step(void);
void  beer_recipe_prev_step(void);
int   beer_recipe_current_step(void);