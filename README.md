# CSSE — Construction Set Extender

Самостоятельный проект расширителя **TES Construction Set** для Morrowind.
Из присланного архива выделены код CSSE, используемые общие классы и ресурсы.
Результат сборки — **CSSE.dll, Windows x86**.

В решении один собираемый проект. MWSE.dll, MWSE-Update, LuaJIT, Boost,
sol, lpeg, yaml-cpp, NuGet и .NET Framework для этой сборки не нужны.
Заголовки fmt, toml11 и DirectX 8 включены в репозиторий; подмодулей нет.

## Сборка на GitHub

1. Создайте репозиторий и загрузите **содержимое** этого каталога в его корень.
   Файл `.github/workflows/build.yml` должен попасть в репозиторий вместе
   с исходниками. При отправке через Git используйте `git add .`.
2. Откройте **Actions → Build CSSE**. Сборка запускается при `push`,
   `pull_request`, а также вручную через **Run workflow**.
3. После успешного завершения откройте **Artifacts** нужного запуска.
   Для установки используйте артефакт **CSSE-Release-Win32**.

Workflow собирает Debug и Release на `windows-2022`, проверяет входные файлы,
архитектуру DLL и её таблицы обычных/отложенных импортов. Результаты:

| Файл внутри артефакта | Назначение |
| --- | --- |
| `CSSE-Release-Win32.zip` | DLL, значки, пример настроек, инструкция и лицензии |
| `CSSE-Release-Win32-symbols.zip` | PDB для диагностики соответствующей DLL |
| `*.sha256` | Контрольные суммы ZIP |

Debug выпускается аналогично с `Debug` в имени. Публикация GitHub Release
автоматически не выполняется; результат доступен в Artifacts.

## Локальная сборка

Нужны **Windows, Visual Studio 2022 / Build Tools, MSVC v143**, Windows SDK
и компонент **C++ MFC for latest build tools (x86 & x64)**.
MFC и CRT связываются статически. Morrowind и установленный редактор
для компиляции не требуются.

Из корня проекта в PowerShell:

```powershell
.\scripts\build.ps1 -Configuration Release
.\scripts\package.ps1 -Configuration Release
```

Готовые файлы: `build\Release\CSSE.dll`, `build\Release\CSSE.pdb`.
Архивы: `dist\Release\`. Для полной пересборки добавьте `-Rebuild`.
Отладочная конфигурация: `-Configuration Debug`.

Можно открыть **CSSE.sln**, выбрать **Release / x86** и собрать решение.
В Developer PowerShell доступна и прямая команда:

```powershell
MSBuild .\CSSE.sln /t:Build /p:Configuration=Release /p:Platform=x86 /m /nr:false
```

При сборке самого `.vcxproj` платформа называется `Win32`:

```powershell
MSBuild .\CSSE\CSSE.vcxproj /t:Build /p:Configuration=Release /p:Platform=Win32 /m /nr:false
```

Для **Visual Studio 2019** с C++ и MFC:

```powershell
.\scripts\build.ps1 -Configuration Release -PlatformToolset v142
```

Скрипт ищет VS 2022 для v143 и VS 2019 для v142. При использовании
v142 внутри VS 2022 вызывайте MSBuild из соответствующего Developer
PowerShell с `/p:PlatformToolset=v142` и установленным MFC для v142.
Поддержка v142 оставлена по инструкции к исходному архиву; CI использует v143.

## Установка и запуск

CSSE расширяет оригинальный **TES Construction Set**. Это DLL,
для её автоматической загрузки нужен **MGE XE 0.14.3 или новее**.
Один файл CSSE.dll сам по себе Windows в редактор не загружает.
Редактор, MGE XE и игровые данные в репозиторий и сборочные архивы не входят.

Закройте редактор и распакуйте runtime-архив рядом с `TES Construction Set.exe`.
Сохраните существующий `csse.toml`. При первом запуске CSSE создаёт полный
файл настроек; в нём должно быть `enabled = true`. Рядом появится `csse.log`.
MWSE.dll для функций редактора не требуется.

Ресурсы автономной версии лежат в `Data Files\CSSE\icons\dark`.
Временный профиль OpenMW — в `Data Files\CSSE\tmp\openmw`.
При переходе со старой сборки свой старый профиль OpenMW можно скопировать
из `Data Files\MWSE\tmp\csse\openmw` в новый каталог.

Кнопка тестирования в OpenMW сохраняется. Укажите папку установленного
OpenMW в `csse.toml`:

```toml
[openmw]
location = 'D:\Games\OpenMW'
```

Кнопка запуска оригинального Morrowind также сохранена. Для передачи
тестового окружения в **оригинальную игру** по `--fromCSSE` по-прежнему
нужна отдельно установленная MWSE и её Lua-модуль CSSE. Они не включены
в этот проект. Это дополнительная интеграция игры, а не зависимость DLL редактора.

По умолчанию сборка ничего не копирует в установленную игру. Если нужна
локальная постсборочная копия DLL/PDB, скопируйте
`config/CSSE.User.props.example` в `config/CSSE.User.props`, укажите
`EditorDir` и включите `CSSEDeployToEditor`. Такой локальный файл исключён из Git.
Ресурсы и пользовательские настройки эта операция не устанавливает.

## Проверка и состав

```powershell
python scripts/verify.py
python scripts/verify.py --binary build/Release/CSSE.dll
```

Для этих дополнительных проверок нужен Python 3.9+. Скрипт использует
только стандартную библиотеку. Проверка исходников не заменяет компиляцию
MSVC и запуск в совместимом Construction Set.

| Каталог/файл | Содержимое |
| --- | --- |
| `CSSE/` | Окна и патчи редактора, ресурсы, DLL-проект |
| `SharedSE/` | Общие классы NetImmerse и низкоуровневые функции, компилируемые внутрь CSSE.dll |
| `deps/` | Только fmt, toml11, заголовки DirectX 8 |
| `config/` | Независимые настройки MSBuild и пример локального развёртывания |
| `scripts/` | Сборка, упаковка и проверка |
| `package/` | Ресурсы и файлы runtime-пакета |
| `docs/EXTRACTION.md` | Происхождение, внесённые изменения и границы проверки |

Общие исходники сохранены с условными ветками для других целей;
автономная сборка явно задаёт `SE_USE_LUA=0`, `SE_IS_MWSE=0`,
`SE_TARGETS_MW=0`, а PCH выбирает `SE_TARGETS_CS=1`.
Эти неактивные ветки не требуют исходников MWSE или библиотек Lua.

## Происхождение и лицензия

Основа — код [MWSE/MWSE](https://github.com/MWSE/MWSE), предоставленный
в `MWSE-master (1).zip`. По приложенной инструкции это снимок коммита
`b4c261b039ee8f5f48d0b8fafe4c33e62903dd4b` с локальными изменениями.
История Git в исходный архив не входила; здесь сохранена атрибуция,
но не заявлена проверка архива на точное совпадение с этим коммитом.

Код CSSE распространяется по **GNU GPL v2** — см. [LICENSE.txt](LICENSE.txt).
Сведения о включённых зависимостях — в [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
