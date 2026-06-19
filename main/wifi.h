#pragma once

// Conecta no Wi-Fi (modo station) usando WIFI_SSID/WIFI_PASS do secrets.h.
// Bloqueia ate obter o primeiro IP. Depois disso reconecta sozinho se cair.
void wifi_init_sta(void);
