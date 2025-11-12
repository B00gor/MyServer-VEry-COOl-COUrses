#!/bin/bash

export BASE_URL="http://localhost:9001"
export TOKEN="eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXUyJ9.eyJlbWFpbCI6ImJvb2dvcmtvbGVua2FAZ21haWwuY29tIiwiZXhwIjoxNzYyNzg5MjA2LCJpYXQiOjE3NjI3MDI4MDYsImlzcyI6ImF1dGgtc2VydmVyIiwicm9sZSI6ItC-0YHQvdC-0LLQsNGC0LXQu9GMIiwidXNlcl9pZCI6ImMwZmZlZWVlLWMwZGUtZWQxZi1jMGRlLWJhZGNhZmVmYWNlZCJ9.uCN5VnBJ05KHhSVult3jqN6Q5XkmMCwI-dWM2xVteZo"

echo "=== Тестирование методов изменения порядка ==="
echo "Base URL: $BASE_URL"
echo ""

# Функция для проверки валидности UUID
check_uuid() {
    local uuid="$1"
    if [[ $uuid =~ ^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$ ]]; then
        echo "✓ Валидный UUID: $uuid"
        return 0
    else
        echo "✗ Невалидный UUID: $uuid"
        return 1
    fi
}

# 1. Создать курс
echo ""
echo "→ POST /courses"
echo "   Создание нового курса для тестирования порядка"
CREATE_COURSE_RESPONSE=$(curl -s -X POST "$BASE_URL/courses" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "title": "Тестовый курс для проверки порядка",
    "description": "Курс для тестирования методов изменения порядка",
    "category": "Программирование",
    "level": "начинающий",
    "language": "ru",
    "price": "0.00",
    "is_paid": false,
    "is_published": true,
    "is_public": true,
    "tags": ["test", "order"]
  }')

echo "$CREATE_COURSE_RESPONSE" | jq

# Проверяем успешность создания курса
if echo "$CREATE_COURSE_RESPONSE" | jq -e '.id' > /dev/null 2>&1; then
    COURSE_ID=$(echo "$CREATE_COURSE_RESPONSE" | jq -r '.id')
    echo "✓ Курс создан успешно, ID: $COURSE_ID"
    check_uuid "$COURSE_ID"
else
    echo "✗ Ошибка создания курса"
    exit 1
fi

# 2. Создать несколько глав
echo ""
echo "→ POST /courses/{id}/chapters (первая глава)"
CREATE_CHAPTER1_RESPONSE=$(curl -s -X POST "$BASE_URL/courses/$COURSE_ID/chapters" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "title": "Глава 1 - Исходный порядок",
    "description": "Первая глава для тестирования порядка",
    "order": 1
  }')

echo "$CREATE_CHAPTER1_RESPONSE" | jq

if echo "$CREATE_CHAPTER1_RESPONSE" | jq -e '.id' > /dev/null 2>&1; then
    CHAPTER1_ID=$(echo "$CREATE_CHAPTER1_RESPONSE" | jq -r '.id')
    echo "✓ Первая глава создана успешно, ID: $CHAPTER1_ID"
    check_uuid "$CHAPTER1_ID"
else
    echo "✗ Ошибка создания первой главы"
    exit 1
fi

echo ""
echo "→ POST /courses/{id}/chapters (вторая глава)"
CREATE_CHAPTER2_RESPONSE=$(curl -s -X POST "$BASE_URL/courses/$COURSE_ID/chapters" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "title": "Глава 2 - Исходный порядок",
    "description": "Вторая глава для тестирования порядка",
    "order": 2
  }')

echo "$CREATE_CHAPTER2_RESPONSE" | jq

if echo "$CREATE_CHAPTER2_RESPONSE" | jq -e '.id' > /dev/null 2>&1; then
    CHAPTER2_ID=$(echo "$CREATE_CHAPTER2_RESPONSE" | jq -r '.id')
    echo "✓ Вторая глава создана успешно, ID: $CHAPTER2_ID"
    check_uuid "$CHAPTER2_ID"
else
    echo "✗ Ошибка создания второй главы"
    exit 1
fi

# 3. Создать видео в курсе (без привязки к главе)
echo ""
echo "→ POST /courses/{id}/videos (первое видео)"
CREATE_VIDEO1_RESPONSE=$(curl -s -X POST "$BASE_URL/courses/$COURSE_ID/videos" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "title": "Видео 1 в курсе",
    "description": "Первое видео для тестирования порядка",
    "order": 1,
    "video_filename": "video1.mp4",
    "video_path": "/videos/test/video1.mp4",
    "duration": "00:10:00",
    "duration_seconds": 600,
    "has_subtitles": false,
    "has_notes": true
  }')

echo "$CREATE_VIDEO1_RESPONSE" | jq

if echo "$CREATE_VIDEO1_RESPONSE" | jq -e '.id' > /dev/null 2>&1; then
    VIDEO1_ID=$(echo "$CREATE_VIDEO1_RESPONSE" | jq -r '.id')
    echo "✓ Первое видео создано успешно, ID: $VIDEO1_ID"
    check_uuid "$VIDEO1_ID"
else
    echo "✗ Ошибка создания первого видео"
    exit 1
fi

echo ""
echo "→ POST /courses/{id}/videos (второе видео)"
CREATE_VIDEO2_RESPONSE=$(curl -s -X POST "$BASE_URL/courses/$COURSE_ID/videos" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "title": "Видео 2 в курсе",
    "description": "Второе видео для тестирования порядка",
    "order": 2,
    "video_filename": "video2.mp4",
    "video_path": "/videos/test/video2.mp4",
    "duration": "00:15:00",
    "duration_seconds": 900,
    "has_subtitles": true,
    "has_notes": false
  }')

echo "$CREATE_VIDEO2_RESPONSE" | jq

if echo "$CREATE_VIDEO2_RESPONSE" | jq -e '.id' > /dev/null 2>&1; then
    VIDEO2_ID=$(echo "$CREATE_VIDEO2_RESPONSE" | jq -r '.id')
    echo "✓ Второе видео создано успешно, ID: $VIDEO2_ID"
    check_uuid "$VIDEO2_ID"
else
    echo "✗ Ошибка создания второго видео"
    exit 1
fi

# 4. Создать видео в главе
echo ""
echo "→ POST /courses/{id}/chapters/{chapterId}/videos (первое видео в главе)"
CREATE_CHAPTER_VIDEO1_RESPONSE=$(curl -s -X POST "$BASE_URL/courses/$COURSE_ID/chapters/$CHAPTER1_ID/videos" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "title": "Видео 1 в главе",
    "description": "Первое видео в главе для тестирования порядка",
    "order": 1,
    "video_filename": "chapter-video1.mp4",
    "video_path": "/videos/test/chapter-video1.mp4",
    "duration": "00:20:00",
    "duration_seconds": 1200,
    "has_subtitles": true,
    "has_notes": false
  }')

echo "$CREATE_CHAPTER_VIDEO1_RESPONSE" | jq

if echo "$CREATE_CHAPTER_VIDEO1_RESPONSE" | jq -e '.id' > /dev/null 2>&1; then
    CHAPTER_VIDEO1_ID=$(echo "$CREATE_CHAPTER_VIDEO1_RESPONSE" | jq -r '.id')
    echo "✓ Первое видео в главе создано успешно, ID: $CHAPTER_VIDEO1_ID"
    check_uuid "$CHAPTER_VIDEO1_ID"
else
    echo "✗ Ошибка создания первого видео в главе"
    exit 1
fi

echo ""
echo "→ POST /courses/{id}/chapters/{chapterId}/videos (второе видео в главе)"
CREATE_CHAPTER_VIDEO2_RESPONSE=$(curl -s -X POST "$BASE_URL/courses/$COURSE_ID/chapters/$CHAPTER1_ID/videos" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "title": "Видео 2 в главе",
    "description": "Второе видео в главе для тестирования порядка",
    "order": 2,
    "video_filename": "chapter-video2.mp4",
    "video_path": "/videos/test/chapter-video2.mp4",
    "duration": "00:25:00",
    "duration_seconds": 1500,
    "has_subtitles": false,
    "has_notes": true
  }')

echo "$CREATE_CHAPTER_VIDEO2_RESPONSE" | jq

if echo "$CREATE_CHAPTER_VIDEO2_RESPONSE" | jq -e '.id' > /dev/null 2>&1; then
    CHAPTER_VIDEO2_ID=$(echo "$CREATE_CHAPTER_VIDEO2_RESPONSE" | jq -r '.id')
    echo "✓ Второе видео в главе создано успешно, ID: $CHAPTER_VIDEO2_ID"
    check_uuid "$CHAPTER_VIDEO2_ID"
else
    echo "✗ Ошибка создания второго видео в главе"
    exit 1
fi

# Пауза перед тестированием методов порядка
sleep 2

echo ""
echo "=== Начинаем тестирование методов изменения порядка ==="

# ТЕСТ 1: Изменить порядок глав
echo ""
echo "=== ТЕСТ 1: PUT /courses/{id}/chapters/order ==="
echo "→ PUT /courses/{id}/chapters/order"
echo "   Изменение порядка следования глав в курсе"
echo "   Course ID: $COURSE_ID"
echo "   Исходный порядок: [$CHAPTER1_ID, $CHAPTER2_ID]"
echo "   Новый порядок: [$CHAPTER2_ID, $CHAPTER1_ID]"
echo "   Request URL: $BASE_URL/courses/$COURSE_ID/chapters/order"
echo "   Request Body: {\"chapter_order\":[\"$CHAPTER2_ID\",\"$CHAPTER1_ID\"]}"

CHAPTER_ORDER_RESPONSE=$(curl -s -X PUT "$BASE_URL/courses/$COURSE_ID/chapters/order" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d "{\"chapter_order\":[\"$CHAPTER2_ID\",\"$CHAPTER1_ID\"]}")

echo "Ответ:"
echo "$CHAPTER_ORDER_RESPONSE" | jq

if echo "$CHAPTER_ORDER_RESPONSE" | jq -e '.message' > /dev/null 2>&1; then
    echo "✓ Порядок глав успешно изменен"
else
    echo "✗ Ошибка изменения порядка глав"
    ERROR_MSG=$(echo "$CHAPTER_ORDER_RESPONSE" | jq -r '.error // .message // "Unknown error"')
    echo "Ошибка: $ERROR_MSG"
    
    # Дополнительная отладочная информация
    echo ""
    echo "=== Отладочная информация для ТЕСТА 1 ==="
    echo "Course ID: $COURSE_ID"
    check_uuid "$COURSE_ID"
    echo "Chapter 1 ID: $CHAPTER1_ID"
    check_uuid "$CHAPTER1_ID"
    echo "Chapter 2 ID: $CHAPTER2_ID"
    check_uuid "$CHAPTER2_ID"
    echo "Полный URL: $BASE_URL/courses/$COURSE_ID/chapters/order"
    echo "Тело запроса:"
    echo "{\"chapter_order\":[\"$CHAPTER2_ID\",\"$CHAPTER1_ID\"]}" | jq
fi

# ТЕСТ 2: Изменить порядок видео в курсе (без глав)
echo ""
echo "=== ТЕСТ 2: PUT /courses/{id}/videos/order ==="
echo "→ PUT /courses/{id}/videos/order"
echo "   Изменение порядка видео, прикреплённых непосредственно к курсу (chapter_id = NULL)"
echo "   Course ID: $COURSE_ID"
echo "   Исходный порядок: [$VIDEO1_ID, $VIDEO2_ID]"
echo "   Новый порядок: [$VIDEO2_ID, $VIDEO1_ID]"
echo "   Request URL: $BASE_URL/courses/$COURSE_ID/videos/order"
echo "   Request Body: {\"video_order\":[\"$VIDEO2_ID\",\"$VIDEO1_ID\"]}"

VIDEO_ORDER_RESPONSE=$(curl -s -X PUT "$BASE_URL/courses/$COURSE_ID/videos/order" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d "{\"video_order\":[\"$VIDEO2_ID\",\"$VIDEO1_ID\"]}")

echo "Ответ:"
echo "$VIDEO_ORDER_RESPONSE" | jq

if echo "$VIDEO_ORDER_RESPONSE" | jq -e '.message' > /dev/null 2>&1; then
    echo "✓ Порядок видео в курсе успешно изменен"
else
    echo "✗ Ошибка изменения порядка видео в курсе"
    ERROR_MSG=$(echo "$VIDEO_ORDER_RESPONSE" | jq -r '.error // .message // "Unknown error"')
    echo "Ошибка: $ERROR_MSG"
    
    # Дополнительная отладочная информация
    echo ""
    echo "=== Отладочная информация для ТЕСТА 2 ==="
    echo "Course ID: $COURSE_ID"
    check_uuid "$COURSE_ID"
    echo "Video 1 ID: $VIDEO1_ID"
    check_uuid "$VIDEO1_ID"
    echo "Video 2 ID: $VIDEO2_ID"
    check_uuid "$VIDEO2_ID"
    echo "Полный URL: $BASE_URL/courses/$COURSE_ID/videos/order"
    echo "Тело запроса:"
    echo "{\"video_order\":[\"$VIDEO2_ID\",\"$VIDEO1_ID\"]}" | jq
fi

# ТЕСТ 3: Изменить порядок видео в главе
echo ""
echo "=== ТЕСТ 3: PUT /courses/{id}/chapters/{chapterId}/videos/order ==="
echo "→ PUT /courses/{id}/chapters/{chapterId}/videos/order"
echo "   Изменение порядка видео внутри конкретной главы"
echo "   Course ID: $COURSE_ID"
echo "   Chapter ID: $CHAPTER1_ID"
echo "   Исходный порядок: [$CHAPTER_VIDEO1_ID, $CHAPTER_VIDEO2_ID]"
echo "   Новый порядок: [$CHAPTER_VIDEO2_ID, $CHAPTER_VIDEO1_ID]"
echo "   Request URL: $BASE_URL/courses/$COURSE_ID/chapters/$CHAPTER1_ID/videos/order"
echo "   Request Body: {\"video_order\":[\"$CHAPTER_VIDEO2_ID\",\"$CHAPTER_VIDEO1_ID\"]}"

CHAPTER_VIDEO_ORDER_RESPONSE=$(curl -s -X PUT "$BASE_URL/courses/$COURSE_ID/chapters/$CHAPTER1_ID/videos/order" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d "{\"video_order\":[\"$CHAPTER_VIDEO2_ID\",\"$CHAPTER_VIDEO1_ID\"]}")

echo "Ответ:"
echo "$CHAPTER_VIDEO_ORDER_RESPONSE" | jq

if echo "$CHAPTER_VIDEO_ORDER_RESPONSE" | jq -e '.message' > /dev/null 2>&1; then
    echo "✓ Порядок видео в главе успешно изменен"
else
    echo "✗ Ошибка изменения порядка видео в главе"
    ERROR_MSG=$(echo "$CHAPTER_VIDEO_ORDER_RESPONSE" | jq -r '.error // .message // "Unknown error"')
    echo "Ошибка: $ERROR_MSG"
    
    # Дополнительная отладочная информация
    echo ""
    echo "=== Отладочная информация для ТЕСТА 3 ==="
    echo "Course ID: $COURSE_ID"
    check_uuid "$COURSE_ID"
    echo "Chapter ID: $CHAPTER1_ID"
    check_uuid "$CHAPTER1_ID"
    echo "Chapter Video 1 ID: $CHAPTER_VIDEO1_ID"
    check_uuid "$CHAPTER_VIDEO1_ID"
    echo "Chapter Video 2 ID: $CHAPTER_VIDEO2_ID"
    check_uuid "$CHAPTER_VIDEO2_ID"
    echo "Полный URL: $BASE_URL/courses/$COURSE_ID/chapters/$CHAPTER1_ID/videos/order"
    echo "Тело запроса:"
    echo "{\"video_order\":[\"$CHAPTER_VIDEO2_ID\",\"$CHAPTER_VIDEO1_ID\"]}" | jq
fi

# ТЕСТ 4: Проверить структуру курса после изменений
echo ""
echo "=== ТЕСТ 4: Проверка структуры курса после изменений ==="
echo "→ GET /courses/{id}/structure"
echo "   Course ID: $COURSE_ID"
echo "   Request URL: $BASE_URL/courses/$COURSE_ID/structure"

STRUCTURE_RESPONSE=$(curl -s -X GET "$BASE_URL/courses/$COURSE_ID/structure" \
  -H "Authorization: Bearer $TOKEN")

echo "Структура курса после изменений порядка:"
echo "$STRUCTURE_RESPONSE" | jq

# Проверяем, что структура содержит ожидаемые элементы
if echo "$STRUCTURE_RESPONSE" | jq -e '.chapters' > /dev/null 2>&1; then
    echo "✓ Структура курса успешно получена"
    
    # Проверяем порядок глав
    CHAPTER_COUNT=$(echo "$STRUCTURE_RESPONSE" | jq '.chapters | length')
    echo "Количество глав в структуре: $CHAPTER_COUNT"
    
    if [ "$CHAPTER_COUNT" -eq 2 ]; then
        FIRST_CHAPTER_ID=$(echo "$STRUCTURE_RESPONSE" | jq -r '.chapters[0].id')
        SECOND_CHAPTER_ID=$(echo "$STRUCTURE_RESPONSE" | jq -r '.chapters[1].id')
        
        echo "Первая глава в структуре: $FIRST_CHAPTER_ID"
        echo "Вторая глава в структуре: $SECOND_CHAPTER_ID"
        
        # Проверяем, соответствует ли порядок ожидаемому (после изменения порядка)
        if [ "$FIRST_CHAPTER_ID" = "$CHAPTER2_ID" ] && [ "$SECOND_CHAPTER_ID" = "$CHAPTER1_ID" ]; then
            echo "✓ Порядок глав в структуре соответствует ожидаемому (глава 2 → глава 1)"
        else
            echo "⚠ Порядок глав в структуре НЕ соответствует ожидаемому"
        fi
    fi
else
    echo "✗ Ошибка получения структуры курса"
fi

# ТЕСТ 5: Ошибочные запросы (невалидные UUID)
echo ""
echo "=== ТЕСТ 5: Тестирование ошибок валидации ==="

# Невалидный UUID для глав
echo ""
echo "→ PUT /courses/{id}/chapters/order (невалидный UUID)"
echo "   Course ID: $COURSE_ID"
echo "   Request Body: {\"chapter_order\":[\"invalid-uuid-123\", \"another-invalid-uuid\"]}"

INVALID_CHAPTER_RESPONSE=$(curl -s -X PUT "$BASE_URL/courses/$COURSE_ID/chapters/order" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"chapter_order":["invalid-uuid-123", "another-invalid-uuid"]}')

echo "Ответ с невалидными UUID глав:"
echo "$INVALID_CHAPTER_RESPONSE" | jq

if echo "$INVALID_CHAPTER_RESPONSE" | jq -e '.error' > /dev/null 2>&1; then
    echo "✓ Корректная обработка невалидных UUID глав"
else
    echo "⚠ Неожиданный ответ на невалидные UUID глав"
fi

# Невалидный UUID для видео
echo ""
echo "→ PUT /courses/{id}/videos/order (невалидный UUID)"
echo "   Course ID: $COURSE_ID"
echo "   Request Body: {\"video_order\":[\"invalid-video-uuid\"]}"

INVALID_VIDEO_RESPONSE=$(curl -s -X PUT "$BASE_URL/courses/$COURSE_ID/videos/order" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"video_order":["invalid-video-uuid"]}')

echo "Ответ с невалидными UUID видео:"
echo "$INVALID_VIDEO_RESPONSE" | jq

if echo "$INVALID_VIDEO_RESPONSE" | jq -e '.error' > /dev/null 2>&1; then
    echo "✓ Корректная обработка невалидных UUID видео"
else
    echo "⚠ Неожиданный ответ на невалидные UUID видео"
fi

# Невалидный UUID для видео в главе
echo ""
echo "→ PUT /courses/{id}/chapters/{chapterId}/videos/order (невалидный UUID)"
echo "   Course ID: $COURSE_ID"
echo "   Chapter ID: $CHAPTER1_ID"
echo "   Request Body: {\"video_order\":[\"invalid-chapter-video-uuid\"]}"

INVALID_CHAPTER_VIDEO_RESPONSE=$(curl -s -X PUT "$BASE_URL/courses/$COURSE_ID/chapters/$CHAPTER1_ID/videos/order" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"video_order":["invalid-chapter-video-uuid"]}')

echo "Ответ с невалидными UUID видео в главе:"
echo "$INVALID_CHAPTER_VIDEO_RESPONSE" | jq

if echo "$INVALID_CHAPTER_VIDEO_RESPONSE" | jq -e '.error' > /dev/null 2>&1; then
    echo "✓ Корректная обработка невалидных UUID видео в главе"
else
    echo "⚠ Неожиданный ответ на невалидные UUID видео в главе"
fi

# Очистка: удалить тестовый курс
echo ""
echo "=== Очистка тестовых данных ==="
echo "→ DELETE /courses/{id}"
echo "   Course ID: $COURSE_ID"
echo "   Request URL: $BASE_URL/courses/$COURSE_ID"

DELETE_RESPONSE=$(curl -s -X DELETE "$BASE_URL/courses/$COURSE_ID" \
  -H "Authorization: Bearer $TOKEN")

echo "Результат удаления курса:"
echo "$DELETE_RESPONSE" | jq

if echo "$DELETE_RESPONSE" | jq -e '.message' > /dev/null 2>&1; then
    echo "✓ Тестовый курс успешно удален"
else
    echo "✗ Ошибка удаления тестового курса"
fi

echo ""
echo "=== Сводка результатов тестирования ==="
echo "Созданные ID:"
echo "  Курс: $COURSE_ID"
echo "  Глава 1: $CHAPTER1_ID"
echo "  Глава 2: $CHAPTER2_ID"
echo "  Видео 1: $VIDEO1_ID"
echo "  Видео 2: $VIDEO2_ID"
echo "  Видео в главе 1: $CHAPTER_VIDEO1_ID"
echo "  Видео в главе 2: $CHAPTER_VIDEO2_ID"

echo ""
echo "=== Тестирование методов изменения порядка завершено ==="
