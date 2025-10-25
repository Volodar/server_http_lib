Micro Todo — каркас микросервисов

Сервисы:
- gateway_service: точка входа (порт 8080).
- users_service: работа с пользователями (порт 8081).
- tasks_service: работа с задачами (порт 8082).
- auth_service: аутентификация (порт 8083), первый запрос создаёт пользователя,
  генерирует пароль (uuid4), сохраняет в БД и отдаёт cookie.

Сборка
- Откройте папку examples/micro_todo как CMake-проект (или добавьте в существующий).
- Цели: gateway_service, users_service, tasks_service.
  Также auth_service.

Запуск (в отдельных терминалах)
- ./gateway_service
- ./users_service
- ./tasks_service
- ./auth_service

Эндпоинты
- GET /health — проверка здоровья сервиса.
- gateway: GET / — список сервисов и портов.
- users: GET /users, POST /users (заглушки).
- tasks: GET /tasks, POST /tasks (заглушки).
- auth: GET /auth — если cookie отсутствует или не найден в БД, создаёт запись
  и устанавливает cookie `auth_token` (HttpOnly, SameSite=Lax). GET /health.
  GET /auth/check — проверка токена (через заголовок `Authorization: Bearer <token>`
  либо cookie/параметр `token`).

Примечания
- CMake добавляет rpath для macOS, чтобы dyld находил OpenSSL и MySQL Connector.
- Для подключения к MySQL задайте переменные окружения: `MYSQL_HOST`,
  `MYSQL_USER`, `MYSQL_PASSWORD`, `MYSQL_DATABASE`. Без них auth_service вернёт 500.
- Конфигурация сервисов: `examples/micro_todo/services.json` (host/port/https для auth).
  Gateway читает этот файл в рабочей директории (CWD). Если файла нет — используются
  дефолтные порты.
- Взаимодействие gateway ↔ auth: gateway валидирует токен через `/auth/check` и
  передаёт его в заголовке `Authorization: Bearer <token>`. При отсутствии или
  неверности токена gateway перенаправляет клиента на `/auth?return=<url назад>`.
