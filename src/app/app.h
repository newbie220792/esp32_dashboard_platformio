#pragma once
// Application wiring: initialises every layer in a fixed order and creates all
// FreeRTOS tasks (stack sizes and priorities live in app.cpp only).
void app_init();
