#pragma once
// =====================================================================
//  Parametros ajustaveis do alimentador de gato.
//  (Credenciais de Wi-Fi e do bot ficam em secrets.h)
// =====================================================================

// ---- Servo SG90 ----------------------------------------------------
// Pino de SINAL do servo (laranja). GND e 5V vao separados.
// OBS: o GPIO2 do ESP32-C3 e um "strapping pin"; costuma funcionar
// normal com o servo, mas se houver problema de boot, e o 1o suspeito.
#define SERVO_GPIO              2

// PWM do servo: 50 Hz (periodo de 20 ms). Padrao de servos analogicos.
#define SERVO_FREQ_HZ           50

// Largura de pulso (em microssegundos) para os extremos do curso.
// O SG90 costuma trabalhar entre ~500us (0 graus) e ~2500us (180 graus).
// Se o servo "bater" no fim do curso, aproxime esses valores (ex.: 600/2400).
#define SERVO_MIN_PULSE_US      500
#define SERVO_MAX_PULSE_US      2500

// Angulos da comporta. CALIBRE conforme a sua montagem mecanica.
#define SERVO_CLOSED_DEG        0     // comporta fechada (posicao de repouso)
#define SERVO_OPEN_DEG          90    // comporta aberta (derrama racao)

// Depois de fechar, cortar o sinal PWM (1) deixa o servo "solto" e evita
// o zumbido/aquecimento parado. Se a racao vazar com a comporta solta,
// troque para 0 para o servo SEGURAR firme a posicao fechada.
#define SERVO_RELAX_WHEN_IDLE   1

// ---- Porcao / seguranca -------------------------------------------
// Tempo padrao (em segundos) de /alimentar quando voce nao passa um numero.
#define DEFAULT_FEED_SECONDS    3

// Teto de seguranca: nunca abre por mais que isso, mesmo se pedirem mais
// (evita esvaziar o pote por engano num comando digitado errado).
#define MAX_FEED_SECONDS        15

// ---- Agenda --------------------------------------------------------
// Quantidade maxima de horarios que da pra cadastrar.
#define MAX_SCHEDULES           10

// ---- Relogio -------------------------------------------------------
// Fuso de Brasilia (UTC-3, sem horario de verao). Formato POSIX TZ.
#define TIMEZONE                "<-03>3"
