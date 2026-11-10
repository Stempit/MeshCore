# Сборка прошивки наблюдателя с нуля

Для машины, на которой этот проект ещё никогда не собирался. Результат —
прошиваемые образы в `out/observer-meshcoretel/<версия>/`.

## 1. Что нужно установить

- **git**
- **Python 3.9+** (под ним работает PlatformIO)
- **PlatformIO Core**:

  ```sh
  pip install -U platformio
  # проверка:
  pio --version
  ```

- ~2 ГБ свободного места (при первой сборке скачиваются тулчейны и фреймворки)
  и доступ в интернет.
- Драйвер USB-serial для ваших плат (macOS: обычно не нужен; Linux: добавьте
  себя в группу `dialout`: `sudo usermod -aG dialout $USER`, затем перелогиньтесь).

## 2. Получаем код

Прошивка наблюдателя живёт в ветке `mqtt-uplink` форка:

```sh
git clone https://github.com/Stempit/MeshCore.git
cd MeshCore
git checkout mqtt-uplink
```

## 3. Собираем

Первая сборка скачивает тулчейн и библиотеки — закладывайте 5–15 минут;
последующие собираются инкрементально.

**Одна плата:**

```sh
pio run -e Heltec_v3_observer_meshcoretel
```

**Все поддерживаемые платы** (рекомендуется — через релизный скрипт):

```sh
./scripts/build-observer_meshcoretel.sh
```

Скрипт: добавляет git-remote на upstream `meshcore-dev/MeshCore`, берёт
последний релизный тег upstream `repeater-v*`, ребейзит `mqtt-uplink` на него,
собирает все окружения `*_observer_meshcoretel` и складывает артефакты.

Успешная сборка выглядит так:

```
RAM:   [==        ]  20.6% (used 67432 bytes from 327680 bytes)
Flash: [====      ]  40.3% (used 1347317 bytes from 3342336 bytes)
========================= [SUCCESS] ...
```

Если RAM или Flash показывают > ~95%, остановитесь и прочитайте
[add-new-device.ru.md](add-new-device.ru.md), раздел «Flash переполнен» —
плате нужна замена таблицы разделов.

## 4. Где лежит прошивка

```
out/observer-meshcoretel/<версия>/<env>/
  <EnvName>-v<версия>-<sha>-merged.bin   ← прошиваем этот файл (bootloader + разделы + прошивка)
  firmware.bin          ← только приложение
  bootloader.bin, partitions.bin
```

`<версия>` — номер релиза upstream, из которого собрана прошивка (например
`1.18.0`).

## 5. Прошиваем плату

Подключите плату по USB и загрузите через PlatformIO:

```sh
pio run -e Heltec_v3_observer_meshcoretel -t upload --upload-port /dev/ttyUSB0
```

(на macOS порты вида `/dev/cu.usbmodem*`; на Windows — `COMx`.)

или прошейте merged-образ напрямую через esptool (имя чипа зависит от платы:
`esp32`, `esp32s3`, `esp32c3`):

```sh
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
    write_flash 0x0 out/observer-meshcoretel/1.18.0/Heltec_v3_observer_meshcoretel/Heltec_v3_observer_meshcoretel-v1.18.0-<sha>-merged.bin
```

`<EnvName>-v<версия>-<sha>-merged.bin` записывает bootloader, таблицу разделов и приложение
одной командой — всегда предпочитайте его ручному подбору смещений.

⚠️ **Никогда не прошивайте один `firmware.bin` на устройство, где стояла
другая прошивка.** Сборки ретранслятора используют разделы OTA0/OTA1:
голый `firmware.bin`, записанный в один слот, загрузчик может проигнорировать
и продолжить загружать *старую* прошивку из другого слота (на экране загрузки
останется старая версия). Если есть сомнения — сначала сотрите флеш и
прошейте merged-образ:

```sh
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 erase_flash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 \
    write_flash 0x0 out/observer-meshcoretel/<версия>/<env>/<EnvName>-v<версия>-<sha>-merged.bin
```

(`erase_flash` стирает и файловую систему — identity и настройки сбрасываются.)
Чтобы убедиться, что загрузилась нужная сборка: на экране загрузки `v1.18.0`,
а `get mqtt.client_version` отвечает `observer_meshcoretel_v0.1.0`.

Браузерная прошивка тоже работает: на `flasher.meshcore.dev` сначала выберите
плату, затем пункт **Custom Firmware** и укажите наш `<EnvName>-v<версия>-<sha>-merged.bin`.
Осторожно: обычный сценарий установки флешера всегда скачивает стоковую
сборку MeshCore (сейчас это v1.17.1 для ретрансляторов) — ваш локальный файл
он не прошивает.

Дальше — [setup.ru.md](setup.ru.md).

## Если что-то не получилось

- **`pio: command not found`** — PlatformIO нет в `PATH`; выполните
  `pip install -U platformio` и откройте консоль заново.
- **Порт не найден / ошибка загрузки** — неверный `--upload-port`, или порт
  занят serial-консолью; закройте её и повторите. Если загрузчик не отвечает —
  зажмите кнопку BOOT на плате в момент старта прошивки.
- **Первая сборка долгая / загрузки падают** — PlatformIO один раз скачивает
  `framework-arduinoespressif32` и тулчейны Xtensa/RISC-V; повторите попытку
  или проверьте сеть/прокси.
- **Конфликты при релизном rebase** — скрипт напечатает инструкцию;
  разрешите `git rebase --continue` (или `--abort`) вручную и запустите снова.
