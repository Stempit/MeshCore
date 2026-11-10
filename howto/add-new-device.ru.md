# Добавление нового устройства в сборку

Прошивка наблюдателя работает на **платах семейства ESP32 с WiFi**
(ESP32 / ESP32-S3 / ESP32-C3). Платы без WiFi (nRF52 — RAK 4631, ThinkNode M1,
STM32, RP2040) поддержать нельзя — для них используйте мост на стороне хоста
(например, `meshcore-go` с интерфейсом PacketPublisher по serial).

## Что нужно заранее

- У платы уже есть рабочее окружение **repeater** в upstream MeshCore:
  проверьте `variants/*/platformio.ini` или выполните
  `pio run --list | grep -i repeater`. Если upstream ещё не поддерживает плату
  как ретранслятор — сначала это вклад в upstream, сюда не входит.

## Шаги

### 1. Добавьте окружение в `observer_meshcoretel.ini`

Допишите блок в конец корневого `observer_meshcoretel.ini`, наследуя его от
существующего repeater-окружения платы:

```ini
[env:<Board>_repeater_observer_meshcoretel]
extends = env:<Board>_repeater
build_flags = ${env:<Board>_repeater.build_flags} -D WITH_MQTT_UPLINK=1
build_src_filter = ${env:<Board>_repeater.build_src_filter} +<helpers/mqtt/*.cpp>
```

Этого достаточно: блок включает код наблюдателя и добавляет
`src/helpers/mqtt/` в сборку. Больше ничего в репозитории менять не нужно —
файл подмешивается в конфиг PlatformIO через `extra_configs` в корневом
`platformio.ini`.

### 2. Соберите

```sh
pio run -e <Board>_repeater_observer_meshcoretel
```

Посмотрите напечатанные проценты **RAM** и **Flash**.

### 3. «Flash переполнен»? (классические ESP32 на 4 МБ)

WiFi + MQTT добавляют к приложению ~170 КБ. Платы, у которых штатная таблица
разделов даёт слот приложения всего 1,25 МБ (типично для классических ESP32 /
4 МБ), переполнятся. Исправление внутри вашего блока:

```ini
; штатный слот приложения 1.25MB не вмещает WiFi+MQTT; min_spiffs даёт слот
; 1.92MB и сохраняет SPIFFS для identity/prefs (цена: нет OTA-слота)
board_build.partitions = min_spiffs.csv
```

Готовые примеры: `LilyGo_TBeam_1W_repeater_observer_meshcoretel` и
`Generic_E22_sx1262_repeater_observer_meshcoretel` в
`observer_meshcoretel.ini`. Учтите цену: плата теряет OTA-слот, а перевод
ноды между раскладками разделов сбрасывает сохранённые настройки (см.
[build-update.ru.md](build-update.ru.md)).

### 4. Зарегистрируйте плату в релизном скрипте

Добавьте имя нового окружения в `ALL_ENVS` в
`scripts/build-observer-meshcoretel.sh`, чтобы релизные сборки его включали.

### 5. Проверьте

```sh
./scripts/build-observer-meshcoretel.sh --env <Board>_repeater_observer_meshcoretel
```

Затем прошейте и настройте узел как обычно ([setup.ru.md](setup.ru.md)) —
настоящая проверка: `get mqtt.status` показывает `meshcoretel:up`, и пакеты
появляются на meshcoretel.ru под вашим IATA.
