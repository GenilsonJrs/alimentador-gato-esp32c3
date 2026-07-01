<h1 align="center">🐱 Alimentador de Gato — ESP32-C3 + Telegram</h1>

<p align="center">
  Alimentador automático controlado por <b>Wi-Fi</b>: sirva ração <b>de qualquer lugar</b>
  por um <b>bot do Telegram</b>, na hora ou em <b>horários agendados</b>. As agendas ficam
  salvas na flash (NVS) e <b>sobrevivem a quedas de energia</b>.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/ESP32--C3-E7352C?logo=espressif&logoColor=white" alt="ESP32-C3">
  <img src="https://img.shields.io/badge/ESP--IDF-v5.x-blue?logo=espressif&logoColor=white" alt="ESP-IDF">
  <img src="https://img.shields.io/badge/Telegram-Bot%20API-26A5E4?logo=telegram&logoColor=white" alt="Telegram Bot">
  <img src="https://img.shields.io/badge/Servo-SG90-FF6F00" alt="Servo SG90">
  <img src="https://img.shields.io/badge/Linguagem-C-00599C?logo=c&logoColor=white" alt="C">
</p>

---

## ✨ O que faz

- 🍽️ **Serve na hora** pelo comando `/alimentar` (porção padrão ou por N segundos).
- ⏰ **Agenda horários fixos** (ex.: todo dia 08:00) que disparam sozinhos.
- 💾 **Persiste as agendas na NVS** — não se perdem ao reiniciar ou faltar energia.
- 🕒 **Relógio via NTP** (fuso de Brasília), garantindo que os horários batam.
- 🔒 **Trava por chat_id**: só o seu Telegram consegue acionar o alimentador.
- 🔌 **Autônomo**: roda em qualquer fonte 5V/USB, sem PC, e reconecta o Wi-Fi sozinho.

## 🧠 Como funciona

O servo gira a comporta para a posição **aberta**, espera os segundos pedidos e volta
para **fechada**. O sinal é um PWM de 50 Hz gerado pelo periférico **LEDC** no **GPIO 2**.

```mermaid
flowchart LR
    U["📱 Você (Telegram)"] -->|internet| API["☁️ api.telegram.org"]
    API <-->|long polling| ESP["📟 ESP32-C3"]
    ESP -->|PWM 50 Hz| S["⚙️ Servo SG90"]
    S -->|abre/fecha comporta| R["🥣 Ração"]
```

## Ligação (hardware)

| SG90        | ESP32-C3        |
|-------------|-----------------|
| Sinal (laranja) | GPIO 2      |
| VCC (vermelho)  | 5V             |
| GND (marrom)    | GND            |

- O sinal de 3,3 V do C3 já aciona o SG90 sem problema.
- **Alimentação:** o SG90 puxa picos de ~500 mA quando força. Se possível, use uma
  fonte 5V dedicada para o servo e **una os GNDs** (servo e ESP no mesmo terra). Se
  alimentar tudo pelo USB e a placa reiniciar sozinha (brown-out), é esse o motivo.
- **GPIO 2 é strapping pin** no C3: normalmente funciona, mas se houver falha de boot,
  é o primeiro item a investigar.

## Configuração

1. Crie um bot: no Telegram fale com **@BotFather**, mande `/newbot` e copie o **token**.
2. Copie os segredos e preencha:
   ```sh
   cp main/secrets.h.example main/secrets.h
   ```
   Edite `main/secrets.h` com seu `WIFI_SSID`, `WIFI_PASS` e `TELEGRAM_BOT_TOKEN`.
   Deixe `TELEGRAM_ALLOWED_CHAT_ID` em `0` por enquanto.
3. Ajustes mecânicos (opcional) em `main/app_config.h`: ângulos de aberto/fechado,
   porção padrão, teto de segurança, etc.

## Compilar e gravar

Com o ESP-IDF instalado (v5.x):

```sh
idf.py set-target esp32c3
idf.py build
idf.py -p COMx flash monitor      # troque COMx pela porta da sua placa
```

## Travar para só você comandar

Na primeira vez, mande qualquer mensagem ao bot: ele responde com o seu **chat_id**.
Cole esse número em `TELEGRAM_ALLOWED_CHAT_ID` (em `secrets.h`) e grave de novo.
A partir daí, só o seu chat consegue acionar o alimentador.

## Comandos do bot

| Comando | O que faz |
|---|---|
| `/alimentar` | Serve a porção padrão (segundos definidos em `app_config.h`) |
| `/alimentar 5` | Serve por 5 segundos |
| `/agendar 08:00 3` | Cadastra: todo dia 08:00, abre por 3 s |
| `/horarios` | Lista os horários cadastrados |
| `/remover 2` | Apaga o horário nº 2 da lista |
| `/status` | Mostra a hora do aparelho e infos |
| `/ajuda` | Mostra a ajuda (e o seu chat_id) |

## Estrutura

```
main/
  main.c         -> inicializa tudo
  servo.c/.h     -> PWM do servo (LEDC) e função dispensar()
  wifi.c/.h      -> conexão Wi-Fi (station), reconecta sozinho
  time_sync.c/.h -> relógio via SNTP (fuso de Brasília)
  schedule.c/.h  -> horários na NVS + tarefa de disparo
  telegram.c/.h  -> long-polling da API do Telegram e comandos
  app_config.h   -> parâmetros ajustáveis
  secrets.h      -> credenciais (NÃO versionado)
```

## Notas

- A agenda dispara comparando a hora local a cada minuto; horários só funcionam
  depois que o relógio sincroniza por NTP (precisa de internet no boot).
- Se a ração vazar com a comporta "solta", coloque `SERVO_RELAX_WHEN_IDLE 0` em
  `app_config.h` para o servo segurar firme a posição fechada.
