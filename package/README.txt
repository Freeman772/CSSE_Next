CSSE — Construction Set Extender (отдельная сборка)
==================================================

Закройте TES Construction Set. Скопируйте CSSE.dll и каталог Data Files
из этого архива в папку, где находится TES Construction Set.exe.

Для загрузки DLL используется установленный MGE XE 0.14.3 или новее.
Загрузчик MGE XE, редактор и игровые данные в этот архив не входят.
MWSE.dll не нужен для функций самого редактора.

При первом запуске CSSE создаст csse.toml и csse.log. Если настройки
уже существуют, сохраните их. Пример csse.toml.example можно переименовать
в csse.toml только при отсутствии собственного файла настроек.

Значки: Data Files\CSSE\icons\dark
Временная конфигурация тестового OpenMW: Data Files\CSSE\tmp\openmw
OpenMW можно указать в csse.toml:

    [openmw]
    location = 'D:\Games\OpenMW'

Кнопка запуска оригинального Morrowind сохраняет исходное поведение.
Передача тестового окружения в оригинальную игру требует отдельно
установленного MWSE и его Lua-модуля CSSE. Они здесь не поставляются.
Запуск OpenMW использует собственную конфигурацию CSSE.

Runtime: CSSE-Release-Win32.zip. Debug — для диагностики.
Файлы PDB поставляются отдельным архивом symbols; для обычной установки
они не обязательны. При диагностике используйте PDB именно от своей DLL.

English: copy CSSE.dll and Data Files next to TES Construction Set.exe.
MGE XE 0.14.3+ is needed to load CSSE. MWSE.dll is not required for editor
features. Keep your existing csse.toml. Game/editor executables, MGE XE
and MWSE are not included. Original-engine test-environment injection
still needs an independently installed MWSE with its CSSE Lua module.

Лицензия кода CSSE: GNU GPL v2, см. LICENSE.txt.
