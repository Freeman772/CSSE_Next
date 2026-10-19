CSSE — Construction Set Extender, Release x86
===========================================

Закройте редактор и скопируйте CSSE.dll рядом с TES Construction Set.exe.
Для загрузки DLL нужен установленный MGE XE 0.14.3+. MWSE.dll для функций
редактора не требуется. Значки уже встроены, внешняя папка значков Data Files
больше не нужна и не читается. PDB поставляется отдельно и не обязателен
для обычной установки.

При первом запуске создаются csse.toml и csse.log. Сохраните существующий
конфиг. Пример csse.toml.example переименовывайте только при отсутствии
своего файла. Тёмная тема включена по умолчанию. Старое значение auto
один раз переводится в dark; явно выбранная светлая тема сохраняется.

Data Files:
- Filter / Ctrl+F: поиск по имени и отображаемым столбцам без учёта регистра.
- Clear / Esc в поле: очистить поиск. Enter переводит фокус к списку.
- Ctrl/Shift+клик, рамка с пустого места: выделить несколько плагинов.
- Ctrl+A: выделить все видимые результаты.
- Check / Uncheck: отметить / снять отметки выделенных файлов.
- Пробел или двойной клик: переключить отметки выделения.
- Перетаскивание группы, стрелки или Alt+Up/Down: поменять порядок.
- Esc во время перетаскивания: отменить перемещение.
- Set As Active File: один выделенный подходящий плагин.

Порядок применяется после OK через даты изменения файлов. Содержимое
ESM/ESP сохраняется. Cancel отменяет ещё не применённый порядок.
Мастера остаются перед плагинами, исходные мастера игры закреплены;
плагин не может перемещаться раньше установленной зависимости.
Фильтр не сбрасывает галочки скрытых файлов.

Копии исходных дат создаются рядом с редактором:
    CSSE\load-order-backups\

Для восстановления закройте редактор и выполните в PowerShell:
    .\Restore-PluginOrder.ps1 -BackupFile '.\CSSE\load-order-backups\ИМЯ_КОПИИ.json'
Скрипт проверяет все файлы и не применяет копию, если их даты уже изменились.

Для OpenMW в csse.toml можно добавить:
    [openmw]
    location = 'D:\Games\OpenMW'
Его временный профиль остаётся в Data Files\CSSE\tmp\openmw.
Внедрение тестового окружения в оригинальный Morrowind требует отдельно
установленной MWSE с игровым Lua-модулем CSSE; они сюда не входят.

English: copy CSSE.dll next to TES Construction Set.exe. MGE XE 0.14.3+ is
required to load it. Icons are embedded; no loose icon directory is used.
Keep your csse.toml. The plugin filter preserves hidden checkmarks.
Ctrl/Shift select groups, Space toggles checks, dragging changes order.
Order changes update file timestamps on OK only, with timestamp backups.

Лицензия кода CSSE: GNU GPL v2, см. LICENSE.txt.
