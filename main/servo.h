#pragma once
#include <stdbool.h>

// Inicializa o LEDC (PWM) e coloca a comporta na posicao FECHADA.
void servo_init(void);

// Move o servo para um angulo (0..180 graus).
void servo_set_angle(int deg);

// Abre a comporta, espera "seconds" segundos e fecha de novo.
// Protegido por mutex: se ja estiver dispensando, retorna false sem fazer nada.
// O tempo e limitado a MAX_FEED_SECONDS (app_config.h).
bool servo_dispense(int seconds);
