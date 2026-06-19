# Alimentador de Gato — ESP32-C3 + Servo SG90 + Telegram

Alimentador automático controlado por **Wi-Fi**, usando **ESP-IDF** em C. Você comanda
de **qualquer lugar** por um **bot do Telegram**: serve ração na hora (por X segundos)
ou cadastra horários fixos. Os horários ficam salvos na flash (NVS) e sobrevivem a
quedas de energia. O relógio é sincronizado por NTP (fuso de Brasília).

## Como funciona

O servo gira a comporta para a posição **aberta**, espera os segundos pedidos e
volta para **fechada**. Tudo é PWM de 50 Hz gerado pelo periférico LEDC no **GPIO 2**.

```
Você (Telegram) --internet--> api.telegram.org <--long polling-- ESP32-C3 --PWM--> Servo SG90
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
