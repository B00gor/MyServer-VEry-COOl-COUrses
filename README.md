# Серверная часть Обучающей Платформы (Drogon C++ Server)

Этот репозиторий содержит серверную часть интерактивной обучающей платформы. Сервер разработан на C++ с использованием высокопроизводительного веб-фреймворка Drogon и отвечает за всю бизнес-логику, взаимодействие с базой данных и предоставление RESTful API для клиентских приложений.

## Описание Проекта

Серверная часть является фундаментом обучающей платформы, обеспечивая следующие ключевые функции:
*   **Управление пользователями**: Регистрация, авторизация, управление профилями (личная информация, контакты, аватар, обложка), изменение ролей.
*   **Управление курсами**: Создание, чтение, обновление, удаление курсов, глав и видеоуроков.
*   **Модерация контента**: Обработка запросов на модерацию видео, одобрение/отклонение контента (функционал ModerationController).
*   **Прогресс обучения**: Отслеживание записи пользователей на курсы и их прогресса по видеоурокам.
*   **Администрирование**: Мониторинг сервера, управление резервным копированием, очистка кэша (функционал AdminController).
*   **Файловый сервис**: Хранение и выдача загруженных файлов (аватары, обложки, видео).

## Основные API-эндпоинты (Controller Groups)

Сервер предоставляет следующие основные группы API:

*   **`AuthController`**:
    *   `POST /auth/register` (Регистрация пользователя)
    *   `POST /auth/login` (Вход в систему)
    *   `POST /auth/refresh` (Обновление JWT токена)
    *   `POST /auth/google` (OAuth через Google)
    *   `POST /auth/logout` (Выход из системы)

*   **`UserController`**:
    *   `GET /users/me` (Получить свой профиль)
    *   `PUT /users/me` (Обновить профиль)
    *   `PUT /users/{id}/role` (Изменить роль пользователя)
    *   API для управления аватаром, обложкой, информацией и контактами профиля.
    *   API для получения и обновления прогресса обучения (`/users/me/progress/courses`, `/users/me/progress/lessons`).

*   **`ChannelController`**:
    *   `GET /channels/{id}` (Получить информацию о канале)
    *   `GET /channels/{id}/courses` (Получить курсы канала)

*   **`CourseController`**:
    *   `GET /courses` (Список курсов с поиском, фильтрацией, пагинацией)
    *   `POST /courses` (Создать курс)
    *   `GET /courses/{id}/structure` (Получить полную структуру курса)
    *   API для управления главами (`/courses/{id}/chapters`).
    *   API для управления видео (уроками) (`/courses/{id}/videos`, `/courses/{id}/chapters/{chapterId}/videos`).
    *   API для записи на курс (`/courses/{id}/enroll`).

*   **`ModerationController` (пока не учитывается в основном ТЗ)**:
    *   `GET /moderation/queue` (Получить очередь модерации)
    *   `PUT /moderation/queue/{id}` (Одобрить/отклонить контент)
    *   API для управления шаблонами модерации.

*   **`AdminController` (пока не учитывается в основном ТЗ)**:
    *   `GET /admin/servers` (Список всех серверов)
    *   `POST /admin/servers/{id}/restart` (Перезагрузить сервер)
    *   API для резервного копирования и управления кэшем.

*   **`AnalyticsController` (пока не учитывается в основном ТЗ)**:
    *   API для получения различной аналитики.

*   **`NotificationController` (пока не учитывается в основном ТЗ)**:
    *   API для управления уведомлениями.

## Структура Проекта

Серверный код организован следующим образом:

*   **`controllers/`**: Содержит исходные файлы контроллеров (`.h` и `.cc`), которые обрабатывают HTTP-запросы для различных API-эндпоинтов (например, `AuthController`, `UserController`, `CourseController`, `ChannelController`). Также включает `FileService` для работы с файловой системой и `JsonCtrl` как базовый контроллер для JSON.
    *   `AuthController.h`, `AuthController.cc`
    *   `UserController.h`, `UserController.cc`
    *   `ChannelController.h`, `ChannelController.cc`
    *   `CourseController.h`, `CourseController.cc`
    *   `FileService.h`, `FileService.cc`
    *   `JsonCtrl.h`, `JsonCtrl.cc`
    *   Вспомогательные библиотеки: `picojson` (для работы с JSON), `jwt-cpp` (для обработки JWT токенов).
    *   `testCourses.sh`: Скрипт для тестирования API.
*   **`models/`**: Содержит ORM-модели (`.h` и `.cc`), которые соответствуют таблицам в базе данных PostgreSQL и обеспечивают взаимодействие с ней.
    *   `Users.h`, `Users.cc`
    *   `UserStats.h`, `UserStats.cc`
    *   `Courses.h`, `Courses.cc`
    *   `CourseChapters.h`, `CourseChapters.cc`
    *   `CourseVideos.h`, `CourseVideos.cc`
    *   `UserProgress.h`, `UserProgress.cc`
    *   `CourseEnrollments.h`, `CourseEnrollments.cc`
    *   `ModerationRequests.h`, `ModerationRequests.cc`
    *   `ModerationTemplates.h`, `ModerationTemplates.cc`
    *   `ServerLogs.h`, `ServerLogs.cc`
    *   `model.json`: Конфигурационный файл для генерации ORM-моделей.
*   **Другие директории**: Могут включать конфигурационные файлы, статические ресурсы, логи и т.д.

## Используемые Технологии

*   **Drogon Framework**: Высокопроизводительный веб-фреймворк на C++ для разработки серверных приложений.
*   **C++**: Основной язык программирования.
*   **PostgreSQL**: Реляционная база данных для хранения всех данных проекта.
*   **`picojson`**: Легковесная библиотека C++ для работы с JSON.
*   **`jwt-cpp`**: Библиотека C++ для создания и верификации JSON Web Tokens.
*   **CMake**: Система сборки проекта.
