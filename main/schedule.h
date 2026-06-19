#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t  hour;       // 0..23
    uint8_t  minute;     // 0..59
    uint16_t seconds;    // tempo de comporta aberta
    uint8_t  enabled;    // 1 = ativo
} feed_schedule_t;

// Carrega a agenda da NVS e sobe a tarefa que dispara nos horarios.
void schedule_init(void);

// Adiciona um horario. Retorna false se a agenda estiver cheia ou dados invalidos.
bool schedule_add(int hour, int minute, int seconds);

// Remove o horario de indice "idx" (base 0). Retorna false se nao existir.
bool schedule_remove(int idx);

// Quantidade de horarios cadastrados.
int schedule_count(void);

// Escreve a lista formatada (texto humano) em "out". Retorna a quantidade.
int schedule_format(char *out, size_t out_size);
