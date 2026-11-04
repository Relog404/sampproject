Папка плагинов сервера.

Здесь должен лежать cef.dll — серверная часть CEF-плагина
aurora-mp/omp-cef v1.4.0 для SA-MP.

Он НЕ хранится в репозитории (сторонний бинарник), а скачивается из корня
проекта:

    powershell -ExecutionPolicy Bypass -File .\setup.ps1 -ServerOnly

Скрипт сверяет SHA-256 архива:
    7bc5d05b086f90708659ef893f0b952eb9fe347497a07845485b5935a69c8d28
    (cef-samp-v1.4.0.zip, 1 113 197 байт)

Скачать вручную (если setup.ps1 не смог):
    1. https://github.com/aurora-mp/omp-cef/releases/tag/v1.4.0
    2. файл cef-samp-v1.4.0.zip  (~1 МБ)
    3. из архива: cef.dll  -> сюда (server\plugins\cef.dll)
                  cef.inc  -> server\pawno\include\cef.inc
    4. проверить: server\tools\check-plugin.cmd

Важно: клиентские файлы (cef.asi, запуск client.dll из cef-samp архива)
сюда класть НЕЛЬЗЯ — это разные двоичники, сервер их не загрузит.

Что будет, если cef.dll нет:

    Loading plugin: cef
     Failed.
    Loaded 0 plugins
    Script[gamemodes/railway.amx]: Run time error 19: "File or function is not found"

Run time error 19 здесь — следствие, а не причина: нативы CEF_* никем не
зарегистрированы. Лечится только установкой плагина.
