Папка плагинов сервера.

Здесь должен появиться cef.dll — серверная часть CEF-плагина
aurora-mp/omp-cef v1.4.0 для SA-MP.

Он НЕ хранится в репозитории (бинарник третьей стороны), а скачивается
скриптом из корня проекта:

    powershell -ExecutionPolicy Bypass -File .\setup.ps1 -ServerOnly

Скрипт проверяет SHA-256 архива:
    7bc5d05b086f90708659ef893f0b952eb9fe347497a07845485b5935a69c8d28

Если cef.dll нет, сервер напишет:
    Failed (plugins/cef.dll)
а гейммод railway.amx не загрузится (Run time error 19).
