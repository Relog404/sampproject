Папка клиента — то, что нужно положить в GTA San Andreas.

Состав:
  cef.asi      — ASI-плагин CEF (загружается ASI loader'ом, напр. dinput8.dll)
  cef\         — рантайм: client.dll, renderer.exe, libcef.dll, locales, resources

Установка (PowerShell в этой папке):

    powershell -ExecutionPolicy Bypass -File .\install-client.ps1 -GtaPath "C:\Games\GTA San Andreas"

Скрипт скачает официальный client-files-v1.4.0.zip (~146 МБ),
проверит SHA-256 и распакует файлы в папку игры.

Без -GtaPath файлы останутся в client\gta-files — скопируйте их
в корень GTA вручную (cef.asi и папку cef\).

Проверка установки:

    powershell -ExecutionPolicy Bypass -File .\check-samp-build.ps1

Скрипт покажет билд samp.dll (R5-2 распознаётся), наличие cef.asi,
папки cef\ и ASI loader'а.

Требуется ASI loader (dinput8.dll от Silent's ASI Loader или аналог):
без него cef.asi не загрузится и CEF-интерфейс не появится.
