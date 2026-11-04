BURBULI / PocketBook PB616W — облачная сборка KEYTEST v0.3
Тестовая прошивка книги: U616.5.17.2182

В АРХИВЕ ИСПОЛНЯЕМОГО .app ПОКА НЕТ.
Компиляция должна происходить в GitHub Actions; результат скачивается из Artifacts.
Сценарий составлен по опубликованным компонентам, но ещё НЕ ЗАПУЩЕН в Actions.

ФАЙЛЫ РЕПОЗИТОРИЯ
  src/pb616_keytest.c            диагностическая программа
  .github/workflows/build.yml    сборка ARM .app через GitHub Actions
  README_RU.txt                  эта инструкция

ЧТО ДЕЛАТЬ НА WINDOWS 10 (НЕ НУЖНО УСТАНАВЛИВАТЬ LINUX/WSL)
1. Откройте https://github.com/new, войдите в GitHub (при необходимости создайте учётную запись).
2. Создайте ПУБЛИЧНЫЙ репозиторий PB616-KEYTEST (никаких секретных данных в архиве нет).
   Если GitHub предлагает README, можете создать его, чтобы появилась страница репозитория.
3. В GitHub: Add file -> Upload files. Из Проводника Windows перетащите
   СОДЕРЖИМОЕ распакованной папки PB616_KEYTEST_v0.3_GITHUB в окно.
   GitHub должен увидеть src/pb616_keytest.c, .github/workflows/build.yml и README_RU.txt.
   Если скрытый каталог .github не добавился, создайте его отдельным шагом:
   Add file -> Create new file -> имя .github/workflows/build.yml
   и вставьте текст build.yml из распакованного архива.
4. Нажмите Commit changes (сохранить файлы в главную ветку).
5. Откройте вкладку Actions -> Build PB616 KEYTEST app -> Run workflow -> Run workflow.
6. Откройте завершившийся запуск. Если зелёный, скачайте внизу страницы
   Artifacts -> PB616_KEYTEST_v0.3_APP. В архиве GitHub будет файл .app.
7. При ошибке сборки: НЕ меняйте ничего на книге. Скопируйте сообщение первой
   ошибки из Actions (шаг с красным значком) и передайте в чат.

ПОСЛЕ УСПЕШНОЙ СБОРКИ (ЭТО ОТДЕЛЬНЫЙ ЭТАП)
- Не заменяйте штатные .app в PocketBook.
- Скопируйте только полученный PB616_KEYTEST_v0.3.app в пользовательскую папку
  applications (устройство может показывать её как Application).
- После запуска проверьте клавиши; программа ведёт журнал
  /mnt/ext1/PB616_KEYTEST_LOG.txt и автоматически завершается через 4 мин.
- Если приложение неожиданно закроется, прочитайте журнал через FAR.

ОГРАНИЧЕНИЯ
- Изготовление .app в реальном GitHub Actions ещё не проверено.
- Runtime-совместимость с конкретной книгой подтвердит только запуск на PB616W.
- Никакой перепрошивки устройства не требуется.

Источники:
https://github.com/koreader/pocketbook-inkview
https://hub.docker.com/r/larento/pocketbook-sdk/tags
https://docs.github.com/en/actions/how-tos/manage-workflow-runs/manually-run-a-workflow
