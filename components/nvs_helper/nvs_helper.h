#pragma once

#include <stddef.h>
#include "nvs_flash.h"
#include "nvs.h"

typedef struct _nvs_helper_schema_def_t
{
  const char*   name;
  nvs_type_t    type;
  size_t        offset;
  int           max_len;
} nvs_helper_schema_def_t;

extern void nvs_helper_init(const char* name);
extern esp_err_t nvs_helper_read_item(const nvs_helper_schema_def_t* item, void* struct_base);
extern esp_err_t nvs_helper_write_item(const nvs_helper_schema_def_t* item, const void* struct_base);
extern esp_err_t nvs_helper_read_all(const nvs_helper_schema_def_t* schema, size_t count, void* base);
extern esp_err_t nvs_helper_read_all_with_defaults(const nvs_helper_schema_def_t* schema, size_t count, void* base, const void* def_base);
extern esp_err_t nvs_helper_write_all(const nvs_helper_schema_def_t* schema, size_t count, const void* base);
