#pragma once
#include <stdbool.h>

// Inicia o SNTP e aplica o fuso (TIMEZONE em app_config.h).
void time_sync_init(void);

// Espera ate o relogio ser sincronizado (ou estourar o timeout).
// Retorna true se a hora ja e valida.
bool time_sync_wait(int timeout_s);
