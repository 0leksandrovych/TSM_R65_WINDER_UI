# HMI homing integration report

## 1. Короткий підсумок

Виконано дві мінімальні production-зміни: загальний період запиту controller telemetry зменшено з 1000 мс до 200 мс, а кнопка START HOMING тепер активна тільки для отриманого від controller стану `HMI_MACHINE_HOMING_REQUIRED`. Наявні mapper, snapshot decoder, travel display, command paths і command-response semantics перевірено без їх редагування.

## 2. Початковий Git-стан

Перед змінами виконано `git status --short`, `git diff --stat` і `git diff`. Усі три команди не вивели змін: робоче дерево було чистим.

## 3. Фінальний `git status --short`

```text
 M components/winder_hmi/src/hmi_config.h
 M components/winder_hmi/src/screens/screen_homing.c
?? hmi_homing_integration_report.md
```

Build-каталог `build_hmi_homing_integration/` ігнорується правилом `.gitignore:6:build_*/` і до Git-стану не потрапив.

## 4. Фінальний `git diff --stat`

```text
 components/winder_hmi/src/hmi_config.h            | 2 +-
 components/winder_hmi/src/screens/screen_homing.c | 2 +-
 2 files changed, 2 insertions(+), 2 deletions(-)
```

`git diff --stat` не показує untracked report, доки його не додано до index.

## 5. Повний список змінених і створених файлів

- `components/winder_hmi/src/hmi_config.h` — змінено один рядок configuration.
- `components/winder_hmi/src/screens/screen_homing.c` — змінено один рядок доступності START HOMING.
- `hmi_homing_integration_report.md` — створено цей звіт; файл залишається untracked.

## 6. Стислий огляд суттєвих diff hunks

- `HMI_TELEMETRY_POLL_INTERVAL_MS`: `1000U` замінено на `200U`.
- У `screen_homing_update()` умову `(required || complete)` замінено на `required`; pending guards залишено без змін.
- Інших production hunks немає. `git diff --check` завершився успішно без виводу.

## 7. Telemetry polling interval

Константу змінено у `components/winder_hmi/src/hmi_config.h`:

```c
#define HMI_TELEMETRY_POLL_INTERVAL_MS  200U
```

Наявний загальний lifecycle у `components/winder_hmi/src/winder_hmi.c` і далі викликає `hmi_controller_client_request_telemetry()` за цією константою для CONNECTING/CONNECTED. Другий polling mechanism не додавався, polling не прив'язувався до Homing screen. Client і codec зберігають шлях `HMI_CONTROLLER_MSG_GET_TELEMETRY -> WINDER_LINK_MSG_GET_TELEMETRY`.

## 8. Доступність START HOMING

`screen_homing_update()` тепер вмикає START лише коли `machine_state_known` і `machine_state == HMI_MACHINE_HOMING_REQUIRED`, а також немає START/ABORT pending. Для `HMI_MACHINE_READY` START недоступний і повторне надсилання з цього екрана заблоковане.

## 9. Логіка CONTINUE

Не змінювалася. Наявна UI-логіка залишає напис `CONTINUE` для authoritative telemetry state `HMI_MACHINE_READY`; для незавершеного homing використовується `BACK HOME`. Job screen або job flow не реалізовувалися.

## 10. Перевірка homing state mapping

У `components/winder_hmi/src/controller/hmi_link_state_mapper.c` перевірено явні mappings для:

- `LINK_MACHINE_STATE_HOMING_REQUIRED`;
- `LINK_MACHINE_STATE_HOMING_SEARCHING_RIGHT_REFERENCE`;
- `LINK_MACHINE_STATE_HOMING_BACKING_OFF_RIGHT_REFERENCE`;
- `LINK_MACHINE_STATE_HOMING_SEARCHING_LEFT_REFERENCE`;
- `LINK_MACHINE_STATE_HOMING_BACKING_OFF_LEFT_REFERENCE`;
- `LINK_MACHINE_STATE_HOMING_MEASURING_TRAVEL`;
- `LINK_MACHINE_STATE_HOMING_APPLYING_OFFSET`;
- `LINK_MACHINE_STATE_HOMING_COMPLETING`;
- `LINK_MACHINE_STATE_READY`.

Критичний mapping існує саме як `LINK_MACHINE_STATE_HOMING_APPLYING_OFFSET -> HMI_MACHINE_HOMING_APPLYING_OFFSET`. Generated controller state IDs у HMI не додавалися.

## 11. Перевірка telemetry decoding

У snapshot codec перевірено bindings `LINK_FIELD_MACHINE_STATE` та `LINK_FIELD_TRAVEL_RANGE_MM`. Для travel задано scale `100.0`, тому wire `7500` декодується як `75.00` мм. RX handler спочатку копіює поточний `hmi_state_t`, потім оновлює тільки поля з відповідним `*_present`; отже snapshot лише з machine state і travel не залежить від наявності optional job fields. Після валідного state mapping встановлюється `machine_state_known`, після travel — `travel_range_known`.

## 12. Перевірка відображення travel

Наявний Homing screen використовує існуючий travel row і після `travel_range_known` форматує `travel_range_mm` як `"%.2f mm"`. Новий widget і повторна physical conversion не додавалися.

## 13. Перевірка START/ABORT command paths

Обидва шляхи перевірено в актуальному коді:

- START: screen callback -> `hmi_actions_start_homing()` -> `hmi_command_bus_emit(HMI_CMD_START_HOMING)` -> client binding `HMI_CONTROLLER_MSG_START_HOMING` -> codec `WINDER_LINK_MSG_START_HOMING`.
- ABORT: screen callback -> `hmi_actions_abort_homing()` -> `hmi_command_bus_emit(HMI_CMD_ABORT_HOMING)` -> client binding `HMI_CONTROLLER_MSG_ABORT_HOMING` -> codec `WINDER_LINK_MSG_ABORT_HOMING`.

ABORT доступний лише для активних detailed homing stages і блокується під час START/ABORT pending. Screen напряму до UART не звертається. Після ABORT локальний state не змінюється — HMI очікує наступний controller snapshot, зокрема `HOMING_REQUIRED`.

## 14. Pending / accepted / rejected semantics

START і ABORT встановлюють відповідний pending перед відправленням команди. `hmi_coordinator_on_command_accepted()` лише перевіряє відповідність pending, очищає його та оновлює navigation; machine state там не змінюється. Homing вважається завершеним тільки після telemetry state `READY`. `hmi_coordinator_on_command_rejected()` очищає pending і записує reason у наявні `last_error`/`last_event`, копіюючи решту поточної моделі, тому machine state зберігається.

## 15. Результат повної збірки

Успішно, exit code `0`. Fresh build для ESP32-S3 завершив усі `1515/1515` кроків, створив `build_hmi_homing_integration/TSM_R65_UI.bin`. Розмір application binary: `0x8ee30` bytes; у найменшій app partition залишилося `0x711d0` bytes (44%).

## 16. Точна build-команда

```powershell
$env:CCACHE_DIR='D:\Git_Repositories\TSM_R65_UI\build_hmi_homing_integration\.ccache'; $env:PATH='D:\Espressif\python_env\idf5.4_py3.11_env\Scripts;' + $env:PATH; Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force; . 'D:\Espressif\frameworks\esp-idf-v5.4.1\export.ps1'; idf.py -B build_hmi_homing_integration build
```

## 17. Warnings, помилки та обмеження

- Build errors: немає.
- Compiler warnings: у зафіксованому build output не спостерігалися.
- ESP-IDF вивів інформаційні `NOTICE` про локально розміщені managed components; це не помилки.
- Перевірки поведінки виконано статичним аналізом актуального коду та повною компіляцією, без прошивання плати й runtime UI/UART test.
- Наявні protocol/codec self-test sources були скомпільовані як частина build, але фактично на target не запускалися.

## 18. Підтвердження меж змін

Не змінювалися controller-проєкт, protocol contract, binary frame format, message IDs, telemetry field IDs, UART transport architecture, command bus architecture, job flow, generated controller files і tests. Job-related model fields, screens та optional decoder code не видалялися.

## Git diff review

- Зміни поза scope: немає.
- Випадкові форматувальні зміни: немає; два production hunks змінюють по одному значенню/виразу.
- Зміни protocol contract: немає.
- Зміни в unrelated screens: немає; змінено лише Homing screen.
- Untracked files: лише `hmi_homing_integration_report.md` у звичайному `git status --short`. Build directory ігнорується наявним правилом.
- Видалені файли: немає.
- Підозрілі або зайві зміни: не виявлено.
- Фінальна whitespace/error перевірка `git diff --check`: успішно, без виводу.
