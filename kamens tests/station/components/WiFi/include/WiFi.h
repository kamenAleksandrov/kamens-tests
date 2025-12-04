#pragma once

/* ======================= WIFI MANAGER HEADER ======================= */
/*
 * This header exposes one function that sets up WiFi as a station.
 * When the ESP32 gets an IP, the manager will turn the LED on
 * and ask the web server module to start.
 */
void wifi_manager_start(void);
