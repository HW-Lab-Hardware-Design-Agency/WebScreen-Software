#pragma once
#include <lvgl.h>
#include <cstdint>

// All access belongs to the LVGL task. Generation bits reject deleted handles
// even after their slots are reused; no mutex or growing vector is needed.
static constexpr unsigned WEBSCREEN_MAX_OBJECTS = 256;
static struct {
  lv_obj_t *object;
  uint32_t generation;
} g_object_slots[WEBSCREEN_MAX_OBJECTS] = {};

static lv_obj_t *get_lv_obj(int handle) {
  if (handle < 0) return nullptr;
  const auto &slot = g_object_slots[(unsigned)handle & 255];
  return slot.generation == ((unsigned)handle >> 8) ? slot.object : nullptr;
}

static void release_subobjects_owned_by(lv_obj_t *root);

static void object_registry_delete_cb(lv_event_t *event) {
  unsigned handle = (unsigned)(uintptr_t)lv_event_get_user_data(event);
  auto &slot = g_object_slots[handle & 255];
  lv_obj_t *object = lv_event_get_target(event);
  if (slot.object != object || slot.generation != (handle >> 8)) return;
  release_subobjects_owned_by(object);
  slot.object = nullptr;
  slot.generation++;  // Retire the slot instead of wrapping at INT32_MAX.
}

static int store_lv_obj(lv_obj_t *object) {
  if (!object) return -1;
  for (unsigned i = 0; i < WEBSCREEN_MAX_OBJECTS; i++) {
    auto &slot = g_object_slots[i];
    if (slot.object || slot.generation > ((unsigned)INT32_MAX >> 8)) continue;
    unsigned handle = (slot.generation << 8) | i;
    if (!lv_obj_add_event_cb(object, object_registry_delete_cb, LV_EVENT_DELETE,
                             (void *)(uintptr_t)handle)) break;
    slot.object = object;
    return (int)handle;
  }
  lv_obj_del(object);
  return -1;
}
