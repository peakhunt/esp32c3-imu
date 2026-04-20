#include <string.h>
#include "esp_log.h"
#include "nvs_helper.h"

static nvs_handle_t _nvs_handle;

esp_err_t
nvs_helper_read_item(const nvs_helper_schema_def_t* item, void* struct_base)
{
  void* dest = (uint8_t*)struct_base + item->offset;
  size_t len = item->max_len;

  switch (item->type)
  {
  case NVS_TYPE_U8:   return nvs_get_u8(_nvs_handle, item->name, (uint8_t*)dest);
  case NVS_TYPE_I8:   return nvs_get_i8(_nvs_handle, item->name, (int8_t*)dest);
  case NVS_TYPE_U16:  return nvs_get_u16(_nvs_handle, item->name, (uint16_t*)dest);
  case NVS_TYPE_I16:  return nvs_get_i16(_nvs_handle, item->name, (int16_t*)dest);
  case NVS_TYPE_U32:  return nvs_get_u32(_nvs_handle, item->name, (uint32_t*)dest);
  case NVS_TYPE_I32:  return nvs_get_i32(_nvs_handle, item->name, (int32_t*)dest);
  case NVS_TYPE_U64:  return nvs_get_u64(_nvs_handle, item->name, (uint64_t*)dest);
  case NVS_TYPE_I64:  return nvs_get_i64(_nvs_handle, item->name, (int64_t*)dest);
  case NVS_TYPE_STR:  return nvs_get_str(_nvs_handle, item->name, (char*)dest, &len);
  case NVS_TYPE_BLOB: return nvs_get_blob(_nvs_handle, item->name, dest, &len);
  default:            return ESP_ERR_NVS_TYPE_MISMATCH;
  }
}

esp_err_t
nvs_helper_write_item(const nvs_helper_schema_def_t* item, const void* struct_base)
{
  const void* src = (const uint8_t*)struct_base + item->offset;

  switch (item->type)
  {
  case NVS_TYPE_U8:   return nvs_set_u8(_nvs_handle, item->name, *(uint8_t*)src);
  case NVS_TYPE_I8:   return nvs_set_i8(_nvs_handle, item->name, *(int8_t*)src);
  case NVS_TYPE_U16:  return nvs_set_u16(_nvs_handle, item->name, *(uint16_t*)src);
  case NVS_TYPE_I16:  return nvs_set_i16(_nvs_handle, item->name, *(int16_t*)src);
  case NVS_TYPE_U32:  return nvs_set_u32(_nvs_handle, item->name, *(uint32_t*)src);
  case NVS_TYPE_I32:  return nvs_set_i32(_nvs_handle, item->name, *(int32_t*)src);
  case NVS_TYPE_U64:  return nvs_set_u64(_nvs_handle, item->name, *(uint64_t*)src);
  case NVS_TYPE_I64:  return nvs_set_i64(_nvs_handle, item->name, *(int64_t*)src);
  case NVS_TYPE_STR:  return nvs_set_str(_nvs_handle, item->name, (const char*)src);
  case NVS_TYPE_BLOB: return nvs_set_blob(_nvs_handle, item->name, src, item->max_len);
  default:            return ESP_ERR_NVS_TYPE_MISMATCH;
  }
}

esp_err_t
nvs_helper_read_all(const nvs_helper_schema_def_t* schema, size_t count, void* base)
{
  for (size_t i = 0; i < count; i++)
  {
    if (nvs_helper_read_item(&schema[i], base) != ESP_OK) return ESP_FAIL;
  }
  return ESP_OK;
}


esp_err_t
nvs_helper_read_all_with_defaults(const nvs_helper_schema_def_t* schema,
    size_t count, void* base, const void* def_base)
{
  bool changed = false;

  for (size_t i = 0; i < count; i++)
  {
    esp_err_t err = nvs_helper_read_item(&schema[i], base);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
      if (nvs_helper_write_item(&schema[i], def_base) != ESP_OK) return ESP_FAIL;
      if (nvs_helper_read_item(&schema[i], base) != ESP_OK) return ESP_FAIL;
      changed = true;
    }
    else if (err != ESP_OK) return err;
  }
  return changed ? nvs_commit(_nvs_handle) : ESP_OK;
}

esp_err_t
nvs_helper_write_all(const nvs_helper_schema_def_t* schema, size_t count, const void* base)
{
  for (size_t i = 0; i < count; i++)
  {
    if (nvs_helper_write_item(&schema[i], base) != ESP_OK) return ESP_FAIL;
  }
  return nvs_commit(_nvs_handle);
}

void
nvs_helper_init(const char* name)
{
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  // If this fails, the device reboots and logs the exact line number. 
  // No silent failures.
  ESP_ERROR_CHECK(nvs_open(name, NVS_READWRITE, &_nvs_handle));
}
