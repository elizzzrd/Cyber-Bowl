#pragma once

using SemaphoreHandle_t = void*;
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return reinterpret_cast<void*>(1); }
inline bool xSemaphoreTake(SemaphoreHandle_t, unsigned) { return true; }
inline void xSemaphoreGive(SemaphoreHandle_t) {}
