#include "CourseController.h"
#include <drogon/HttpResponse.h>
#include <drogon/utils/Utilities.h>
#include <drogon/drogon.h>
#include <numeric>
#include <algorithm>
#include <jwt-cpp/jwt.h>

using namespace drogon;
using namespace drogon::orm;
using namespace std;

// Вспомогательная функция для создания JSON ответов
Json::Value CourseController::createJsonResponse(const string& key, const string& value) {
    Json::Value json;
    json[key] = value;
    return json;
}

// Валидация UUID
bool CourseController::isValidUUID(const string& uuid) {
    if (uuid.empty()) return false;

    // Базовые проверки
    if (uuid.length() != 36) return false;
    if (uuid[8] != '-' || uuid[13] != '-' || uuid[18] != '-' || uuid[23] != '-') return false;

    // Проверка hex символов
    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) continue;
        char c = uuid[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }

    return true;
}

// Валидация данных курса
bool CourseController::isValidCourseData(const Json::Value& data) {
    if (!data.isMember("title") || data["title"].asString().empty()) {
        return false;
    }
    if (!data.isMember("category") || data["category"].asString().empty()) {
        return false;
    }
    if (!data.isMember("level") || data["level"].asString().empty()) {
        return false;
    }
    return true;
}

// Валидация данных главы
bool CourseController::isValidChapterData(const Json::Value& data) {
    if (!data.isMember("title") || data["title"].asString().empty()) {
        return false;
    }
    if (!data.isMember("order") || !data["order"].isInt()) {
        return false;
    }
    return true;
}

// Валидация данных видео
bool CourseController::isValidVideoData(const Json::Value& data) {
    if (!data.isMember("title") || data["title"].asString().empty()) {
        return false;
    }
    if (!data.isMember("video_filename") || data["video_filename"].asString().empty()) {
        return false;
    }
    if (!data.isMember("video_path") || data["video_path"].asString().empty()) {
        return false;
    }
    return true;
}

// JWT аутентификация
string CourseController::getCurrentUserId(const HttpRequestPtr& req) {
    auto authHeader = req->getHeader("Authorization");
    if (authHeader.empty() || authHeader.find("Bearer ") != 0) {
        return "";
    }

    string token = authHeader.substr(7);
    return getUserIdFromToken(token);
}

bool CourseController::validateJWT(const string& token) {
    try {
        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify()
                            .allow_algorithm(jwt::algorithm::hs256{JWT_SECRET})
                            .with_issuer("auth-server");
        verifier.verify(decoded);
        return true;
    } catch (const exception& e) {
        LOG_ERROR << "JWT validation error: " << e.what();
        return false;
    }
}

string CourseController::getUserIdFromToken(const string& token) {
    try {
        if (!validateJWT(token)) {
            return "";
        }

        auto decoded = jwt::decode(token);

        // Пытаемся получить user_id из claims
        try {
            auto user_id_claim = decoded.get_payload_claim("user_id");
            return user_id_claim.as_string();
        } catch (const exception& e) {
            LOG_DEBUG << "user_id claim not found: " << e.what();
        }

        // Альтернативный способ: парсим payload как JSON
        string payload_str = decoded.get_payload();
        Json::Value payload;
        Json::Reader reader;

        if (reader.parse(payload_str, payload) && payload.isMember("user_id")) {
            return payload["user_id"].asString();
        }
    } catch (const exception& e) {
        LOG_ERROR << "JWT get user_id error: " << e.what();
    }
    return "";
}

string CourseController::getRoleFromToken(const string& token) {
    try {
        if (!validateJWT(token)) {
            return "";
        }

        auto decoded = jwt::decode(token);

        // Пытаемся получить role из claims
        try {
            auto role_claim = decoded.get_payload_claim("role");
            return role_claim.as_string();
        } catch (const exception& e) {
            LOG_DEBUG << "role claim not found: " << e.what();
        }

        // Альтернативный способ: парсим payload как JSON
        string payload_str = decoded.get_payload();
        Json::Value payload;
        Json::Reader reader;

        if (reader.parse(payload_str, payload) && payload.isMember("role")) {
            return payload["role"].asString();
        }
    } catch (const exception& e) {
        LOG_ERROR << "JWT get role error: " << e.what();
    }
    return "";
}

bool CourseController::hasPermission(const HttpRequestPtr& req, const vector<string>& allowedRoles) {
    auto authHeader = req->getHeader("Authorization");
    if (authHeader.empty() || authHeader.find("Bearer ") != 0) {
        return false;
    }

    string token = authHeader.substr(7);
    string userRole = getRoleFromToken(token);

    return find(allowedRoles.begin(), allowedRoles.end(), userRole) != allowedRoles.end();
}

// Вспомогательный метод для создания JSON ответа курса
Json::Value CourseController::getCourseJsonResponse(const Courses& course) {
    Json::Value courseJson;

    try {
        courseJson["id"] = course.getValueOfId();
        courseJson["title"] = course.getValueOfTitle();
        courseJson["description"] = course.getValueOfDescription();
        courseJson["category"] = course.getValueOfCategory();
        courseJson["level"] = course.getValueOfLevel();
        courseJson["language"] = course.getValueOfLanguage();
        courseJson["author_id"] = course.getValueOfAuthorId();

        courseJson["chapters_count"] = course.getValueOfChaptersCount();
        courseJson["videos_count"] = course.getValueOfVideosCount();
        courseJson["total_views"] = course.getValueOfTotalViews();
        courseJson["total_likes"] = course.getValueOfTotalLikes();

        courseJson["price"] = course.getValueOfPrice();
        courseJson["is_paid"] = course.getValueOfIsPaid();
        courseJson["is_published"] = course.getValueOfIsPublished();
        courseJson["is_public"] = course.getValueOfIsPublic();
        courseJson["rating"] = course.getValueOfRating();

        // Обработка nullable полей
        try {
            string coverPath = course.getValueOfCoverPath();
            if (!coverPath.empty()) {
                courseJson["cover_path"] = coverPath;
            } else {
                courseJson["cover_path"] = Json::nullValue;
            }
        } catch (const exception& e) {
            courseJson["cover_path"] = Json::nullValue;
        }

        try {
            string iconPath = course.getValueOfIconPath();
            if (!iconPath.empty()) {
                courseJson["icon_path"] = iconPath;
            } else {
                courseJson["icon_path"] = Json::nullValue;
            }
        } catch (const exception& e) {
            courseJson["icon_path"] = Json::nullValue;
        }

        // Парсим теги
        try {
            string tagsStr = course.getValueOfTags();
            if (!tagsStr.empty()) {
                Json::Value tags;
                Json::Reader reader;
                if (reader.parse(tagsStr, tags)) {
                    courseJson["tags"] = tags;
                } else {
                    courseJson["tags"] = Json::arrayValue;
                }
            } else {
                courseJson["tags"] = Json::arrayValue;
            }
        } catch (const exception& e) {
            courseJson["tags"] = Json::arrayValue;
        }

        courseJson["created_at"] = course.getValueOfCreatedAt().toDbStringLocal();
        courseJson["updated_at"] = course.getValueOfUpdatedAt().toDbStringLocal();

        try {
            auto lastAccessed = course.getValueOfLastAccessedAt();
            if (lastAccessed != trantor::Date()) {
                courseJson["last_accessed_at"] = lastAccessed.toDbStringLocal();
            } else {
                courseJson["last_accessed_at"] = Json::nullValue;
            }
        } catch (const exception& e) {
            courseJson["last_accessed_at"] = Json::nullValue;
        }

    } catch (const exception& e) {
        LOG_ERROR << "Error creating course JSON response: " << e.what();
    }

    return courseJson;
}

// Вспомогательный метод для создания JSON ответа главы
Json::Value CourseController::getChapterJsonResponse(const CourseChapters& chapter) {
    Json::Value chapterJson;

    try {
        chapterJson["id"] = chapter.getValueOfId();
        chapterJson["course_id"] = chapter.getValueOfCourseId();
        chapterJson["title"] = chapter.getValueOfTitle();
        chapterJson["description"] = chapter.getValueOfDescription();
        chapterJson["order"] = chapter.getValueOfOrder();
        chapterJson["videos_count"] = chapter.getValueOfVideosCount();
        chapterJson["total_duration"] = chapter.getValueOfTotalDuration();
        chapterJson["created_at"] = chapter.getValueOfCreatedAt().toDbStringLocal();
        chapterJson["updated_at"] = chapter.getValueOfUpdatedAt().toDbStringLocal();
    } catch (const exception& e) {
        LOG_ERROR << "Error creating chapter JSON response: " << e.what();
    }

    return chapterJson;
}

// Вспомогательный метод для создания JSON ответа видео
Json::Value CourseController::getVideoJsonResponse(const CourseVideos& video) {
    Json::Value videoJson;

    try {
        videoJson["id"] = video.getValueOfId();
        videoJson["course_id"] = video.getValueOfCourseId();

        // Исправление: проверяем, пустая ли строка вместо isNull()
        string chapterId = video.getValueOfChapterId();
        videoJson["chapter_id"] = chapterId.empty() ? Json::Value(Json::nullValue) : Json::Value(chapterId);
        videoJson["author_id"] = video.getValueOfAuthorId();
        videoJson["title"] = video.getValueOfTitle();
        videoJson["description"] = video.getValueOfDescription();
        videoJson["order"] = video.getValueOfOrder();
        videoJson["video_filename"] = video.getValueOfVideoFilename();
        videoJson["video_path"] = video.getValueOfVideoPath();
        videoJson["duration"] = video.getValueOfDuration();
        videoJson["duration_seconds"] = video.getValueOfDurationSeconds();
        videoJson["cover_path"] = video.getValueOfCoverPath();
        videoJson["has_subtitles"] = video.getValueOfHasSubtitles();
        videoJson["has_notes"] = video.getValueOfHasNotes();
        videoJson["views_count"] = video.getValueOfViewsCount();
        videoJson["likes_count"] = video.getValueOfLikesCount();
        videoJson["is_approved"] = video.getValueOfIsApproved();

        // Поля модерации
        try {
            string approvedBy = video.getValueOfApprovedBy();
            if (!approvedBy.empty()) {
                videoJson["approved_by"] = approvedBy;
            } else {
                videoJson["approved_by"] = Json::nullValue;
            }
        } catch (const exception& e) {
            videoJson["approved_by"] = Json::nullValue;
        }

        try {
            auto approvedAt = video.getValueOfApprovedAt();
            if (approvedAt != trantor::Date()) {
                videoJson["approved_at"] = approvedAt.toDbStringLocal();
            } else {
                videoJson["approved_at"] = Json::nullValue;
            }
        } catch (const exception& e) {
            videoJson["approved_at"] = Json::nullValue;
        }

        try {
            string uploadedBy = video.getValueOfUploadedBy();
            if (!uploadedBy.empty()) {
                videoJson["uploaded_by"] = uploadedBy;
            } else {
                videoJson["uploaded_by"] = Json::nullValue;
            }
        } catch (const exception& e) {
            videoJson["uploaded_by"] = Json::nullValue;
        }

        videoJson["created_at"] = video.getValueOfCreatedAt().toDbStringLocal();
        videoJson["updated_at"] = video.getValueOfUpdatedAt().toDbStringLocal();

        try {
            auto lastAccessed = video.getValueOfLastAccessedAt();
            if (lastAccessed != trantor::Date()) {
                videoJson["last_accessed_at"] = lastAccessed.toDbStringLocal();
            } else {
                videoJson["last_accessed_at"] = Json::nullValue;
            }
        } catch (const exception& e) {
            videoJson["last_accessed_at"] = Json::nullValue;
        }

    } catch (const exception& e) {
        LOG_ERROR << "Error creating video JSON response: " << e.what();
    }

    return videoJson;
}

// Проверка, является ли пользователь автором курса
bool CourseController::isCourseAuthor(const string& userId, const string& courseId) {
    auto dbClient = app().getDbClient();

    try {
        auto result = dbClient->execSqlSync("SELECT author_id FROM courses WHERE id = $1", courseId);
        if (!result.empty()) {
            return result[0]["author_id"].as<string>() == userId;
        }
    } catch (const exception& e) {
        LOG_ERROR << "Error checking course author: " << e.what();
    }

    return false;
}

// Проверка, является ли пользователь автором видео
bool CourseController::isVideoAuthor(const string& userId, const string& videoId) {
    auto dbClient = app().getDbClient();

    try {
        auto result = dbClient->execSqlSync("SELECT author_id FROM course_videos WHERE id = $1", videoId);
        if (!result.empty()) {
            return result[0]["author_id"].as<string>() == userId;
        }
    } catch (const exception& e) {
        LOG_ERROR << "Error checking video author: " << e.what();
    }

    return false;
}

// Проверка, записан ли пользователь на курс
bool CourseController::isEnrolledInCourse(const string& userId, const string& courseId) {
    auto dbClient = app().getDbClient();

    try {
        auto result = dbClient->execSqlSync("SELECT * FROM course_enrollments WHERE user_id = $1 AND course_id = $2", userId, courseId);
        return !result.empty();
    } catch (const exception& e) {
        LOG_ERROR << "Error checking enrollment: " << e.what();
    }

    return false;
}

// =============================================================================
// МЕТОДЫ ДЛЯ РАБОТЫ С ФАЙЛАМИ
// =============================================================================

CourseController::FileInfo CourseController::saveCoverImage(const HttpRequestPtr& req,
                                                            const string& courseId,
                                                            const string& chapterId) {
    FileInfo fileInfo;

    // Используем MultiPartParser для получения файлов
    MultiPartParser fileUpload;
    if (fileUpload.parse(req) != 0) {
        return fileInfo;
    }

    auto files = fileUpload.getFiles();

    // Ищем файл обложки (второй файл в запросе или с определенным именем)
    for (size_t i = 0; i < files.size(); ++i) {
        const auto& file = files[i];
        string name = file.getFileName();
        if (name.find("cover") != string::npos ||
            name.find("image") != string::npos ||
            name.find("jpg") != string::npos ||
            name.find("jpeg") != string::npos ||
            name.find("png") != string::npos) {

            string savePath;
            if (chapterId.empty()) {
                savePath = baseUploadPath_ + "/courses/" + courseId + "/covers";
            } else {
                savePath = baseUploadPath_ + "/courses/" + courseId + "/chapters/" + chapterId + "/covers";
            }

            if (!ensureDirectoryExists(savePath)) {
                throw runtime_error("Failed to create directory: " + savePath);
            }

            string filename = generateFilename(file.getFileName());
            string fullPath = savePath + "/" + filename;

            // Сохраняем файл - ИСПРАВЛЕНО: используем saveAs() вместо saveTo()
            file.saveAs(fullPath);

            fileInfo.filename = filename;
            fileInfo.path = fullPath.substr(baseUploadPath_.length() + 1);
            fileInfo.full_path = fullPath;
            // ИСПРАВЛЕНО: используем length() вместо getFileSize()
            fileInfo.size = file.fileLength();
            fileInfo.mime_type = getMimeType(filename);

            break;
        }
    }

    return fileInfo;
}

bool CourseController::deleteFile(const string& path) {
    try {
        if (fs::exists(path)) {
            return fs::remove(path);
        }
        return true; // Файл и так не существует
    } catch (const exception& e) {
        LOG_ERROR << "Error deleting file: " << e.what();
        return false;
    }
}

string CourseController::getFilePath(const string& courseId,
                                     const string& chapterId,
                                     const string& filename,
                                     bool isCover) {
    string path;
    if (chapterId.empty()) {
        path = baseUploadPath_ + "/courses/" + courseId +
               (isCover ? "/covers/" : "/videos/") + filename;
    } else {
        path = baseUploadPath_ + "/courses/" + courseId + "/chapters/" + chapterId +
               (isCover ? "/covers/" : "/videos/") + filename;
    }
    return path;
}

string CourseController::generateFilename(const string& originalName) {
    // Извлекаем расширение файла
    size_t dotPos = originalName.find_last_of('.');
    string extension = (dotPos != string::npos) ? originalName.substr(dotPos) : "";

    // Генерируем UUID для имени файла
    string uuid = drogon::utils::getUuid();

    return uuid + extension;
}

string CourseController::getMimeType(const string& filename) {
    size_t dotPos = filename.find_last_of('.');
    if (dotPos == string::npos) return "application/octet-stream";

    string ext = filename.substr(dotPos + 1);
    transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == "mp4") return "video/mp4";
    if (ext == "avi") return "video/x-msvideo";
    if (ext == "mov") return "video/quicktime";
    if (ext == "webm") return "video/webm";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "png") return "image/png";
    if (ext == "gif") return "image/gif";

    return "application/octet-stream";
}

// =============================================================================
// ОСНОВНЫЕ МЕТОДЫ КОНТРОЛЛЕРА
// =============================================================================

// GET /courses - Список курсов с поиском, фильтрацией, пагинацией
void CourseController::getCourses(const HttpRequestPtr& req,
                                  function<void(const HttpResponsePtr&)>&& callback) {

    // Получаем параметры запроса
    auto paramsMap = req->getParameters();
    // ИСПРАВЛЕНО: правильное получение параметров из unordered_map
    string pageStr = paramsMap.find("page") != paramsMap.end() ? paramsMap.at("page") : "1";
    string limitStr = paramsMap.find("limit") != paramsMap.end() ? paramsMap.at("limit") : "10";
    string search = paramsMap.find("search") != paramsMap.end() ? paramsMap.at("search") : "";
    string category = paramsMap.find("category") != paramsMap.end() ? paramsMap.at("category") : "";
    string level = paramsMap.find("level") != paramsMap.end() ? paramsMap.at("level") : "";
    string sortBy = paramsMap.find("sort_by") != paramsMap.end() ? paramsMap.at("sort_by") : "created_at";
    string sortOrder = paramsMap.find("sort_order") != paramsMap.end() ? paramsMap.at("sort_order") : "desc";

    // Валидация параметров
    int page, limit;
    try {
        page = stoi(pageStr);
        limit = stoi(limitStr);
        if (page < 1) page = 1;
        if (limit < 1 || limit > 100) limit = 10;
    } catch (const exception& e) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid pagination parameters"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    int offset = (page - 1) * limit;

    auto dbClient = app().getDbClient();

    // Строим SQL запрос для получения курсов
    string sql = "SELECT * FROM courses WHERE is_published = true ";
    vector<string> params;

    if (!search.empty()) {
        sql += "AND (title ILIKE $1 OR description ILIKE $1) ";
        params.push_back("%" + search + "%");
    }

    if (!category.empty()) {
        sql += "AND category = $" + to_string(params.size() + 1) + " ";
        params.push_back(category);
    }

    if (!level.empty()) {
        sql += "AND level = $" + to_string(params.size() + 1) + " ";
        params.push_back(level);
    }

    // Добавляем сортировку
    vector<string> allowedSortFields = {"created_at", "updated_at", "title", "rating", "total_views", "price"};
    string safeSortBy = find(allowedSortFields.begin(), allowedSortFields.end(), sortBy) != allowedSortFields.end()
                            ? sortBy : "created_at";
    string safeSortOrder = (sortOrder == "asc" || sortOrder == "desc") ? sortOrder : "desc";

    sql += "ORDER BY " + safeSortBy + " " + safeSortOrder + " ";
    sql += "LIMIT $" + to_string(params.size() + 1) + " OFFSET $" + to_string(params.size() + 2);
    params.push_back(to_string(limit));
    params.push_back(to_string(offset));

    // Строим SQL запрос для подсчета
    string countSql = "SELECT COUNT(*) as total FROM courses WHERE is_published = true";
    vector<string> countParams;

    if (!search.empty()) {
        countSql += " AND (title ILIKE $1 OR description ILIKE $1)";
        countParams.push_back("%" + search + "%");
    }

    if (!category.empty()) {
        countSql += " AND category = $" + to_string(countParams.size() + 1);
        countParams.push_back(category);
    }

    if (!level.empty()) {
        countSql += " AND level = $" + to_string(countParams.size() + 1);
        countParams.push_back(level);
    }

    auto callbackPtr = std::make_shared<std::function<void(const HttpResponsePtr&)>>(std::move(callback));

    // Выполняем запрос на подсчет с правильными параметрами
    auto executeCountQuery = [dbClient, sql, params, countParams, page, limit, callbackPtr, this, &countSql](auto&&... countArgs) {
        dbClient->execSqlAsync(countSql,
                               [dbClient, sql, params, page, limit, callbackPtr, this](const Result& countResult) {
                                   int total = countResult[0]["total"].as<int>();

                                   // Выполняем запрос на получение курсов
                                   auto coursesCallback = [callbackPtr, total, page, limit, this](const Result& coursesResult) {
                                       Json::Value response;
                                       Json::Value coursesArray(Json::arrayValue);

                                       for (const auto& row : coursesResult) {
                                           try {
                                               auto course = Courses(row);
                                               coursesArray.append(getCourseJsonResponse(course));
                                           } catch (const exception& e) {
                                               LOG_ERROR << "Error processing course data: " << e.what();
                                           }
                                       }

                                       response["courses"] = coursesArray;
                                       response["pagination"] = Json::Value();
                                       response["pagination"]["page"] = page;
                                       response["pagination"]["limit"] = limit;
                                       response["pagination"]["total"] = total;
                                       response["pagination"]["pages"] = (total + limit - 1) / limit;

                                       auto resp = HttpResponse::newHttpJsonResponse(response);
                                       (*callbackPtr)(resp);
                                   };

                                   auto errorCallback = [callbackPtr, this](const DrogonDbException& e) {
                                       LOG_ERROR << "Database error fetching courses: " << e.base().what();
                                       auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                                       resp->setStatusCode(k500InternalServerError);
                                       (*callbackPtr)(resp);
                                   };

                                   // Выполняем запрос в зависимости от количества параметров
                                   switch (params.size()) {
                                   case 0:
                                       dbClient->execSqlAsync(sql, coursesCallback, errorCallback);
                                       break;
                                   case 1:
                                       dbClient->execSqlAsync(sql, coursesCallback, errorCallback, params[0]);
                                       break;
                                   case 2:
                                       dbClient->execSqlAsync(sql, coursesCallback, errorCallback, params[0], params[1]);
                                       break;
                                   case 3:
                                       dbClient->execSqlAsync(sql, coursesCallback, errorCallback, params[0], params[1], params[2]);
                                       break;
                                   case 4:
                                       dbClient->execSqlAsync(sql, coursesCallback, errorCallback, params[0], params[1], params[2], params[3]);
                                       break;
                                   default:
                                       auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Too many parameters"));
                                       resp->setStatusCode(k400BadRequest);
                                       (*callbackPtr)(resp);
                                   }
                               },
                               [callbackPtr, this](const DrogonDbException& e) {
                                   LOG_ERROR << "Database error counting courses: " << e.base().what();
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                                   resp->setStatusCode(k500InternalServerError);
                                   (*callbackPtr)(resp);
                               },
                               countArgs...);
    };

    // Вызываем подсчет с правильным количеством параметров
    switch (countParams.size()) {
    case 0:
        executeCountQuery();
        break;
    case 1:
        executeCountQuery(countParams[0]);
        break;
    case 2:
        executeCountQuery(countParams[0], countParams[1]);
        break;
    case 3:
        executeCountQuery(countParams[0], countParams[1], countParams[2]);
        break;
    default:
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Too many parameters"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
    }
}

// POST /courses - Создать курс
void CourseController::createCourse(const HttpRequestPtr& req,
                                    function<void(const HttpResponsePtr&)>&& callback) {

    string userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    // Проверяем, что пользователь имеет право создавать курсы (основатель или пользователь)
    if (!hasPermission(req, {"основатель", "пользователь"})) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Insufficient permissions"));
        resp->setStatusCode(k403Forbidden);
        callback(resp);
        return;
    }

    Json::Value jsonBody;
    Json::Reader reader;

    string body = string(req->getBody());
    if (!reader.parse(body, jsonBody)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid JSON"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Валидация данных курса
    if (!isValidCourseData(jsonBody)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Missing required fields: title, category, level"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    try {
        string sql = R"(
            INSERT INTO courses (author_id, title, description, category, level, language, cover_path, icon_path, price, is_paid, is_published, is_public, tags)
            VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13)
            RETURNING id
        )";

        // Подготавливаем значения
        string title = jsonBody["title"].asString();
        string description = jsonBody.get("description", "").asString();
        string category = jsonBody["category"].asString();
        string level = jsonBody["level"].asString();
        string language = jsonBody.get("language", "ru").asString();
        string coverPath = jsonBody.get("cover_path", "").asString();
        string iconPath = jsonBody.get("icon_path", "").asString();
        string price = jsonBody.get("price", "0").asString();
        bool isPaid = jsonBody.get("is_paid", false).asBool();
        bool isPublished = jsonBody.get("is_published", false).asBool();
        bool isPublic = jsonBody.get("is_public", true).asBool();

        string tags = "[]";
        if (jsonBody.isMember("tags")) {
            Json::StreamWriterBuilder writer;
            tags = Json::writeString(writer, jsonBody["tags"]);
        }

        dbClient->execSqlAsync(sql,
                               [callback](const Result& result) {
                                   Json::Value response;
                                   response["id"] = result[0]["id"].as<string>();
                                   response["message"] = "Course created successfully";

                                   auto resp = HttpResponse::newHttpJsonResponse(response);
                                   resp->setStatusCode(k201Created);
                                   callback(resp);
                               },
                               [callback, this](const DrogonDbException& e) {
                                   LOG_ERROR << "Database error creating course: " << e.base().what();
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Failed to create course"));
                                   resp->setStatusCode(k500InternalServerError);
                                   callback(resp);
                               },
                               userId, title, description, category, level, language, coverPath, iconPath, price, isPaid, isPublished, isPublic, tags
                               );

    } catch (const exception& e) {
        LOG_ERROR << "Error creating course: " << e.what();
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid course data"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
    }
}

// GET /courses/{id} - Получить курс по ID
void CourseController::getCourseById(const HttpRequestPtr& req,
                                     function<void(const HttpResponsePtr&)>&& callback,
                                     const string& courseId) {

    if (!isValidUUID(courseId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid course ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();
    string userId = getCurrentUserId(req);

    // Получаем роль пользователя для проверки прав доступа
    string userRole = getRoleFromToken(req->getHeader("Authorization").substr(7));
    bool hasAdminAccess = (userRole == "основатель" || userRole == "админ");

    dbClient->execSqlAsync("SELECT * FROM courses WHERE id = $1",
                           [userId, hasAdminAccess, callback, this](const Result& result) {
                               if (result.empty()) {
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Course not found"));
                                   resp->setStatusCode(k404NotFound);
                                   callback(resp);
                                   return;
                               }

                               try {
                                   auto course = Courses(result[0]);

                                   // Проверяем доступ к курсу
                                   bool isPublished = course.getValueOfIsPublished();
                                   bool isPublic = course.getValueOfIsPublic();
                                   bool isAuthor = (userId == course.getValueOfAuthorId());

                                   if (!isPublished && !isAuthor && !hasAdminAccess) {
                                       auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Course not published"));
                                       resp->setStatusCode(k403Forbidden);
                                       callback(resp);
                                       return;
                                   }

                                   if (!isPublic && !isAuthor && !hasAdminAccess && !isEnrolledInCourse(userId, course.getValueOfId())) {
                                       auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Course is private"));
                                       resp->setStatusCode(k403Forbidden);
                                       callback(resp);
                                       return;
                                   }

                                   auto courseJson = getCourseJsonResponse(course);
                                   auto resp = HttpResponse::newHttpJsonResponse(courseJson);
                                   callback(resp);

                               } catch (const exception& e) {
                                   LOG_ERROR << "Error processing course data: " << e.what();
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Error processing course data"));
                                   resp->setStatusCode(k500InternalServerError);
                                   callback(resp);
                               }
                           },
                           [callback, this](const DrogonDbException& e) {
                               LOG_ERROR << "Database error: " << e.base().what();
                               auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                               resp->setStatusCode(k500InternalServerError);
                               callback(resp);
                           },
                           courseId
                           );
}

// PUT /courses/{id} - Обновить курс
void CourseController::updateCourse(const HttpRequestPtr& req,
                                    function<void(const HttpResponsePtr&)>&& callback,
                                    const string& courseId) {

    string userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    if (!isValidUUID(courseId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid course ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Проверяем, что пользователь является автором курса или имеет права администратора
    if (!isCourseAuthor(userId, courseId) && !hasPermission(req, {"основатель", "админ"})) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Forbidden - only course author can update course"));
        resp->setStatusCode(k403Forbidden);
        callback(resp);
        return;
    }

    Json::Value jsonBody;
    Json::Reader reader;
    string body = string(req->getBody());
    if (!reader.parse(body, jsonBody)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid JSON"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    // Сначала проверяем существование курса
    dbClient->execSqlAsync("SELECT * FROM courses WHERE id = $1",
                           [dbClient, jsonBody, courseId, callback, this](const Result& courseResult) {
                               if (courseResult.empty()) {
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Course not found"));
                                   resp->setStatusCode(k404NotFound);
                                   callback(resp);
                                   return;
                               }

                               // Строим SQL запрос для обновления
                               string sql = "UPDATE courses SET ";
                               vector<string> updates;
                               vector<string> params;

                               if (jsonBody.isMember("title")) {
                                   updates.push_back("title = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["title"].asString());
                               }

                               if (jsonBody.isMember("description")) {
                                   updates.push_back("description = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["description"].asString());
                               }

                               if (jsonBody.isMember("category")) {
                                   updates.push_back("category = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["category"].asString());
                               }

                               if (jsonBody.isMember("level")) {
                                   updates.push_back("level = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["level"].asString());
                               }

                               if (jsonBody.isMember("language")) {
                                   updates.push_back("language = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["language"].asString());
                               }

                               if (jsonBody.isMember("cover_path")) {
                                   updates.push_back("cover_path = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["cover_path"].asString());
                               }

                               if (jsonBody.isMember("icon_path")) {
                                   updates.push_back("icon_path = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["icon_path"].asString());
                               }

                               if (jsonBody.isMember("price")) {
                                   updates.push_back("price = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["price"].asString());
                               }

                               if (jsonBody.isMember("is_paid")) {
                                   updates.push_back("is_paid = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["is_paid"].asBool() ? "true" : "false");
                               }

                               if (jsonBody.isMember("is_published")) {
                                   updates.push_back("is_published = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["is_published"].asBool() ? "true" : "false");
                               }

                               if (jsonBody.isMember("is_public")) {
                                   updates.push_back("is_public = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["is_public"].asBool() ? "true" : "false");
                               }

                               if (jsonBody.isMember("tags")) {
                                   Json::StreamWriterBuilder writer;
                                   string tags = Json::writeString(writer, jsonBody["tags"]);
                                   updates.push_back("tags = $" + to_string(params.size() + 1));
                                   params.push_back(tags);
                               }

                               if (updates.empty()) {
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "No fields to update"));
                                   resp->setStatusCode(k400BadRequest);
                                   callback(resp);
                                   return;
                               }

                               sql += std::accumulate(std::next(updates.begin()), updates.end(), updates[0],
                                                      [](const string& a, const string& b) { return a + ", " + b; });
                               sql += ", updated_at = CURRENT_TIMESTAMP WHERE id = $" + to_string(params.size() + 1);
                               params.push_back(courseId);

                               // Выполняем обновление
                               auto executeUpdate = [sql, params, callback, this](auto&&... args) {
                                   auto dbClient = app().getDbClient();
                                   dbClient->execSqlAsync(sql,
                                                          [callback](const Result& result) {
                                                              Json::Value response;
                                                              response["message"] = "Course updated successfully";
                                                              auto resp = HttpResponse::newHttpJsonResponse(response);
                                                              callback(resp);
                                                          },
                                                          [callback, this](const DrogonDbException& e) {
                                                              LOG_ERROR << "Database error updating course: " << e.base().what();
                                                              auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Failed to update course"));
                                                              resp->setStatusCode(k500InternalServerError);
                                                              callback(resp);
                                                          },
                                                          args...);
                               };

                               // Вызываем с правильным количеством параметров
                               switch (params.size()) {
                               case 1: executeUpdate(params[0]); break;
                               case 2: executeUpdate(params[0], params[1]); break;
                               case 3: executeUpdate(params[0], params[1], params[2]); break;
                               case 4: executeUpdate(params[0], params[1], params[2], params[3]); break;
                               case 5: executeUpdate(params[0], params[1], params[2], params[3], params[4]); break;
                               case 6: executeUpdate(params[0], params[1], params[2], params[3], params[4], params[5]); break;
                               case 7: executeUpdate(params[0], params[1], params[2], params[3], params[4], params[5], params[6]); break;
                               case 8: executeUpdate(params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7]); break;
                               case 9: executeUpdate(params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7], params[8]); break;
                               case 10: executeUpdate(params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7], params[8], params[9]); break;
                               case 11: executeUpdate(params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7], params[8], params[9], params[10]); break;
                               case 12: executeUpdate(params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7], params[8], params[9], params[10], params[11]); break;
                               default:
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Too many parameters"));
                                   resp->setStatusCode(k400BadRequest);
                                   callback(resp);
                               }
                           },
                           [callback, this](const DrogonDbException& e) {
                               LOG_ERROR << "Database error checking course: " << e.base().what();
                               auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                               resp->setStatusCode(k500InternalServerError);
                               callback(resp);
                           },
                           courseId
                           );
}

// DELETE /courses/{id} - Удалить курс
void CourseController::deleteCourse(const HttpRequestPtr& req,
                                    function<void(const HttpResponsePtr&)>&& callback,
                                    const string& courseId) {

    string userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    if (!isValidUUID(courseId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid course ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Проверяем, что пользователь является автором курса или имеет права администратора
    if (!isCourseAuthor(userId, courseId) && !hasPermission(req, {"основатель", "админ"})) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Forbidden - only course author can delete course"));
        resp->setStatusCode(k403Forbidden);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    // Сначала проверяем существование курса
    dbClient->execSqlAsync("SELECT * FROM courses WHERE id = $1",
                           [dbClient, courseId, callback, this](const Result& courseResult) {
                               if (courseResult.empty()) {
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Course not found"));
                                   resp->setStatusCode(k404NotFound);
                                   callback(resp);
                                   return;
                               }

                               // Удаляем курс (каскадное удаление должно быть настроено в БД для связанных записей)
                               dbClient->execSqlAsync("DELETE FROM courses WHERE id = $1",
                                                      [callback](const Result& result) {
                                                          Json::Value response;
                                                          response["message"] = "Course deleted successfully";

                                                          auto resp = HttpResponse::newHttpJsonResponse(response);
                                                          callback(resp);
                                                      },
                                                      [callback, this](const DrogonDbException& e) {
                                                          LOG_ERROR << "Database error deleting course: " << e.base().what();
                                                          auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Failed to delete course"));
                                                          resp->setStatusCode(k500InternalServerError);
                                                          callback(resp);
                                                      },
                                                      courseId
                                                      );
                           },
                           [callback, this](const DrogonDbException& e) {
                               LOG_ERROR << "Database error checking course: " << e.base().what();
                               auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                               resp->setStatusCode(k500InternalServerError);
                               callback(resp);
                           },
                           courseId
                           );
}

// GET /courses/{id}/structure - Полная структура курса
void CourseController::getCourseStructure(const HttpRequestPtr& req,
                                          function<void(const HttpResponsePtr&)>&& callback,
                                          const string& courseId) {

    if (!isValidUUID(courseId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid course ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();
    string userId = getCurrentUserId(req);

    // Получаем роль пользователя для проверки прав доступа
    string userRole = getRoleFromToken(req->getHeader("Authorization").substr(7));
    bool hasAdminAccess = (userRole == "основатель" || userRole == "админ");

    // Сначала проверяем доступ к курсу
    dbClient->execSqlAsync("SELECT * FROM courses WHERE id = $1",
                           [dbClient, userId, hasAdminAccess, callback, courseId, this](const Result& courseResult) {
                               if (courseResult.empty()) {
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Course not found"));
                                   resp->setStatusCode(k404NotFound);
                                   callback(resp);
                                   return;
                               }

                               try {
                                   auto course = Courses(courseResult[0]);

                                   // Проверяем доступ к курсу
                                   bool isPublished = course.getValueOfIsPublished();
                                   bool isPublic = course.getValueOfIsPublic();
                                   bool isAuthor = (userId == course.getValueOfAuthorId());

                                   if (!isPublished && !isAuthor && !hasAdminAccess) {
                                       auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Course not published"));
                                       resp->setStatusCode(k403Forbidden);
                                       callback(resp);
                                       return;
                                   }

                                   if (!isPublic && !isAuthor && !hasAdminAccess && !isEnrolledInCourse(userId, course.getValueOfId())) {
                                       auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Course is private"));
                                       resp->setStatusCode(k403Forbidden);
                                       callback(resp);
                                       return;
                                   }

                                   // Получаем главы курса
                                   dbClient->execSqlAsync(
                                       "SELECT * FROM course_chapters WHERE course_id = $1 ORDER BY \"order\" ASC",
                                       [dbClient, courseId, callback, this](const Result& chaptersResult) {
                                           // Исправление: создаем структуру внутри лямбды
                                           Json::Value structure;
                                           Json::Value chaptersArray(Json::arrayValue);

                                           // Если глав нет, получаем видео без глав
                                           if (chaptersResult.empty()) {
                                               dbClient->execSqlAsync(
                                                   "SELECT * FROM course_videos WHERE course_id = $1 AND chapter_id IS NULL AND is_approved = true ORDER BY \"order\" ASC",
                                                   [callback, this](const Result& videosResult) {
                                                       Json::Value structure;
                                                       Json::Value videosArray(Json::arrayValue);
                                                       for (const auto& row : videosResult) {
                                                           try {
                                                               auto video = CourseVideos(row);
                                                               videosArray.append(this->getVideoJsonResponse(video));
                                                           } catch (const exception& e) {
                                                               LOG_ERROR << "Error processing video data: " << e.what();
                                                           }
                                                       }
                                                       structure["videos_without_chapters"] = videosArray;

                                                       auto resp = HttpResponse::newHttpJsonResponse(structure);
                                                       callback(resp);
                                                   },
                                                   [callback, this](const DrogonDbException& e) {
                                                       LOG_ERROR << "Database error fetching videos: " << e.base().what();
                                                       auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                                                       resp->setStatusCode(k500InternalServerError);
                                                       callback(resp);
                                                   },
                                                   courseId
                                                   );
                                               return;
                                           }

                                           // Исправление: используем shared_ptr для корректного захвата
                                           auto structurePtr = std::make_shared<Json::Value>();
                                           auto chaptersArrayPtr = std::make_shared<Json::Value>(Json::arrayValue);
                                           auto remainingChapters = std::make_shared<int>(chaptersResult.size());

                                           for (const auto& chapterRow : chaptersResult) {
                                               try {
                                                   auto chapter = CourseChapters(chapterRow);
                                                   auto chapterJsonPtr = std::make_shared<Json::Value>(this->getChapterJsonResponse(chapter));
                                                   string chapterId = chapter.getValueOfId();

                                                   // Получаем видео для этой главы
                                                   dbClient->execSqlAsync(
                                                       "SELECT * FROM course_videos WHERE chapter_id = $1 AND is_approved = true ORDER BY \"order\" ASC",
                                                       [chapterJsonPtr, chaptersArrayPtr, remainingChapters, structurePtr, callback, this](
                                                           const Result& videosResult) {

                                                           Json::Value videosArray(Json::arrayValue);
                                                           for (const auto& videoRow : videosResult) {
                                                               try {
                                                                   auto video = CourseVideos(videoRow);
                                                                   videosArray.append(this->getVideoJsonResponse(video));
                                                               } catch (const exception& e) {
                                                                   LOG_ERROR << "Error processing video data: " << e.what();
                                                               }
                                                           }
                                                           (*chapterJsonPtr)["videos"] = videosArray;
                                                           chaptersArrayPtr->append(*chapterJsonPtr);

                                                           (*remainingChapters)--;
                                                           if (*remainingChapters == 0) {
                                                               (*structurePtr)["chapters"] = *chaptersArrayPtr;
                                                               auto resp = HttpResponse::newHttpJsonResponse(*structurePtr);
                                                               callback(resp);
                                                           }
                                                       },
                                                       [callback, this](const DrogonDbException& e) {
                                                           LOG_ERROR << "Database error fetching chapter videos: " << e.base().what();
                                                           auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                                                           resp->setStatusCode(k500InternalServerError);
                                                           callback(resp);
                                                       },
                                                       chapterId
                                                       );
                                               } catch (const exception& e) {
                                                   LOG_ERROR << "Error processing chapter data: " << e.what();
                                                   (*remainingChapters)--;
                                                   if (*remainingChapters == 0) {
                                                       (*structurePtr)["chapters"] = *chaptersArrayPtr;
                                                       auto resp = HttpResponse::newHttpJsonResponse(*structurePtr);
                                                       callback(resp);
                                                   }
                                               }
                                           }
                                       },
                                       [callback, this](const DrogonDbException& e) {
                                           LOG_ERROR << "Database error fetching chapters: " << e.base().what();
                                           auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                                           resp->setStatusCode(k500InternalServerError);
                                           callback(resp);
                                       },
                                       courseId
                                       );

                               } catch (const exception& e) {
                                   LOG_ERROR << "Error processing course data: " << e.what();
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Error processing course data"));
                                   resp->setStatusCode(k500InternalServerError);
                                   callback(resp);
                               }
                           },
                           [callback, this](const DrogonDbException& e) {
                               LOG_ERROR << "Database error: " << e.base().what();
                               auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                               resp->setStatusCode(k500InternalServerError);
                               callback(resp);
                           },
                           courseId
                           );
}

// POST /courses/{id}/enroll - Записаться на курс
void CourseController::enrollInCourse(const HttpRequestPtr& req,
                                      function<void(const HttpResponsePtr&)>&& callback,
                                      const string& courseId) {

    string userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    if (!isValidUUID(courseId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid course ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    // Проверяем существование курса и его публикацию
    dbClient->execSqlAsync("SELECT * FROM courses WHERE id = $1 AND is_published = true",
                           [dbClient, userId, courseId, callback, this](const Result& courseResult) {
                               if (courseResult.empty()) {
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Course not found or not published"));
                                   resp->setStatusCode(k404NotFound);
                                   callback(resp);
                                   return;
                               }

                               // Проверяем, не записан ли уже пользователь
                               dbClient->execSqlAsync("SELECT * FROM course_enrollments WHERE user_id = $1 AND course_id = $2",
                                                      [dbClient, userId, courseId, callback, this](const Result& enrollmentResult) {
                                                          if (!enrollmentResult.empty()) {
                                                              auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Already enrolled in this course"));
                                                              resp->setStatusCode(k400BadRequest);
                                                              callback(resp);
                                                              return;
                                                          }

                                                          // Исправление: используем явный SQL запрос вместо метода insert
                                                          string sql = R"(
                        INSERT INTO course_enrollments (user_id, course_id, completion_percentage, is_completed)
                        VALUES ($1, $2, $3, $4)
                        RETURNING id
                    )";

                                                          dbClient->execSqlAsync(sql,
                                                                                 [callback](const Result& result) {
                                                                                     Json::Value response;
                                                                                     response["message"] = "Successfully enrolled in course";

                                                                                     auto resp = HttpResponse::newHttpJsonResponse(response);
                                                                                     resp->setStatusCode(k201Created);
                                                                                     callback(resp);
                                                                                 },
                                                                                 [callback, this](const DrogonDbException& e) {
                                                                                     LOG_ERROR << "Database error enrolling in course: " << e.base().what();
                                                                                     auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Failed to enroll in course"));
                                                                                     resp->setStatusCode(k500InternalServerError);
                                                                                     callback(resp);
                                                                                 },
                                                                                 userId, courseId, 0, false
                                                                                 );
                                                      },
                                                      [callback, this](const DrogonDbException& e) {
                                                          LOG_ERROR << "Database error checking enrollment: " << e.base().what();
                                                          auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                                                          resp->setStatusCode(k500InternalServerError);
                                                          callback(resp);
                                                      },
                                                      userId, courseId
                                                      );
                           },
                           [callback, this](const DrogonDbException& e) {
                               LOG_ERROR << "Database error checking course: " << e.base().what();
                               auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                               resp->setStatusCode(k500InternalServerError);
                               callback(resp);
                           },
                           courseId
                           );
}

// GET /courses/enrolled - Курсы, на которые я записан
void CourseController::getEnrolledCourses(const HttpRequestPtr& req,
                                          function<void(const HttpResponsePtr&)>&& callback) {

    string userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    dbClient->execSqlAsync(
        "SELECT c.*, ce.completion_percentage, ce.is_completed, ce.last_accessed_at "
        "FROM courses c "
        "JOIN course_enrollments ce ON c.id = ce.course_id "
        "WHERE ce.user_id = $1 "
        "ORDER BY ce.last_accessed_at DESC",
        [callback, this](const Result& result) {
            Json::Value response;
            Json::Value coursesArray(Json::arrayValue);

            for (const auto& row : result) {
                try {
                    auto course = Courses(row);
                    Json::Value courseJson = getCourseJsonResponse(course);
                    courseJson["completion_percentage"] = row["completion_percentage"].as<int>();
                    courseJson["is_completed"] = row["is_completed"].as<bool>();
                    courseJson["last_accessed_at"] = row["last_accessed_at"].as<string>();

                    coursesArray.append(courseJson);
                } catch (const exception& e) {
                    LOG_ERROR << "Error processing enrolled course data: " << e.what();
                }
            }

            response["courses"] = coursesArray;
            auto resp = HttpResponse::newHttpJsonResponse(response);
            callback(resp);
        },
        [callback, this](const DrogonDbException& e) {
            LOG_ERROR << "Database error fetching enrolled courses: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        userId
        );
}

// GET /courses/{id}/chapters - Получить главы курса
void CourseController::getChapters(const HttpRequestPtr& req,
                                   function<void(const HttpResponsePtr&)>&& callback,
                                   const string& courseId) {

    if (!isValidUUID(courseId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid course ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();
    string userId = getCurrentUserId(req);

    // Проверяем доступ к курсу
    dbClient->execSqlAsync("SELECT * FROM courses WHERE id = $1",
                           [dbClient, userId, callback, courseId, this, &req](const Result& courseResult) {
                               if (courseResult.empty()) {
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Course not found"));
                                   resp->setStatusCode(k404NotFound);
                                   callback(resp);
                                   return;
                               }

                               try {
                                   auto course = Courses(courseResult[0]);

                                   // Проверяем доступ к курсу
                                   bool isPublished = course.getValueOfIsPublished();
                                   bool isPublic = course.getValueOfIsPublic();
                                   bool isAuthor = (userId == course.getValueOfAuthorId());
                                   string userRole = getRoleFromToken(req->getHeader("Authorization").substr(7));
                                   bool hasAdminAccess = (userRole == "основатель" || userRole == "админ");

                                   if (!isPublished && !isAuthor && !hasAdminAccess) {
                                       auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Course not published"));
                                       resp->setStatusCode(k403Forbidden);
                                       callback(resp);
                                       return;
                                   }

                                   if (!isPublic && !isAuthor && !hasAdminAccess && !isEnrolledInCourse(userId, course.getValueOfId())) {
                                       auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Course is private"));
                                       resp->setStatusCode(k403Forbidden);
                                       callback(resp);
                                       return;
                                   }

                                   // Получаем главы курса
                                   dbClient->execSqlAsync(
                                       "SELECT * FROM course_chapters WHERE course_id = $1 ORDER BY \"order\" ASC",
                                       [callback, this](const Result& chaptersResult) {
                                           Json::Value response;
                                           Json::Value chaptersArray(Json::arrayValue);

                                           for (const auto& row : chaptersResult) {
                                               try {
                                                   auto chapter = CourseChapters(row);
                                                   chaptersArray.append(getChapterJsonResponse(chapter));
                                               } catch (const exception& e) {
                                                   LOG_ERROR << "Error processing chapter data: " << e.what();
                                               }
                                           }

                                           response["chapters"] = chaptersArray;
                                           auto resp = HttpResponse::newHttpJsonResponse(response);
                                           callback(resp);
                                       },
                                       [callback, this](const DrogonDbException& e) {
                                           LOG_ERROR << "Database error fetching chapters: " << e.base().what();
                                           auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                                           resp->setStatusCode(k500InternalServerError);
                                           callback(resp);
                                       },
                                       courseId
                                       );

                               } catch (const exception& e) {
                                   LOG_ERROR << "Error processing course data: " << e.what();
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Error processing course data"));
                                   resp->setStatusCode(k500InternalServerError);
                                   callback(resp);
                               }
                           },
                           [callback, this](const DrogonDbException& e) {
                               LOG_ERROR << "Database error: " << e.base().what();
                               auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                               resp->setStatusCode(k500InternalServerError);
                               callback(resp);
                           },
                           courseId
                           );
}

// POST /courses/{id}/chapters - Создать главу
void CourseController::createChapter(const HttpRequestPtr& req,
                                     function<void(const HttpResponsePtr&)>&& callback,
                                     const string& courseId) {

    string userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    if (!isValidUUID(courseId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid course ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Проверяем, что пользователь является автором курса или имеет права администратора
    if (!isCourseAuthor(userId, courseId) && !hasPermission(req, {"основатель", "админ"})) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Forbidden - only course author can modify chapters"));
        resp->setStatusCode(k403Forbidden);
        callback(resp);
        return;
    }

    Json::Value jsonBody;
    Json::Reader reader;
    string body = string(req->getBody());
    if (!reader.parse(body, jsonBody)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid JSON"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Валидация обязательных полей
    if (!isValidChapterData(jsonBody)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Missing required fields: title, order"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    try {
        string sql = R"(
            INSERT INTO course_chapters (course_id, title, description, "order")
            VALUES ($1, $2, $3, $4)
            RETURNING id
        )";

        string title = jsonBody["title"].asString();
        string description = jsonBody.get("description", "").asString();
        int order = jsonBody["order"].asInt();

        dbClient->execSqlAsync(sql,
                               [callback](const Result& result) {
                                   Json::Value response;
                                   response["id"] = result[0]["id"].as<string>();
                                   response["message"] = "Chapter created successfully";

                                   auto resp = HttpResponse::newHttpJsonResponse(response);
                                   resp->setStatusCode(k201Created);
                                   callback(resp);
                               },
                               [callback, this](const DrogonDbException& e) {
                                   LOG_ERROR << "Database error creating chapter: " << e.base().what();
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Failed to create chapter"));
                                   resp->setStatusCode(k500InternalServerError);
                                   callback(resp);
                               },
                               courseId, title, description, order
                               );

    } catch (const exception& e) {
        LOG_ERROR << "Error creating chapter: " << e.what();
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid chapter data"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
    }
}

// PUT /courses/{id}/chapters/{chapterId} - Обновить главу
void CourseController::updateChapter(const HttpRequestPtr& req,
                                     function<void(const HttpResponsePtr&)>&& callback,
                                     const string& courseId,
                                     const string& chapterId) {

    string userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    if (!isValidUUID(courseId) || !isValidUUID(chapterId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid course or chapter ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Проверяем, что пользователь является автором курса или имеет права администратора
    if (!isCourseAuthor(userId, courseId) && !hasPermission(req, {"основатель", "админ"})) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Forbidden - only course author can modify chapters"));
        resp->setStatusCode(k403Forbidden);
        callback(resp);
        return;
    }

    Json::Value jsonBody;
    Json::Reader reader;
    string body = string(req->getBody());
    if (!reader.parse(body, jsonBody)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid JSON"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    // Сначала проверяем существование главы
    dbClient->execSqlAsync("SELECT * FROM course_chapters WHERE id = $1 AND course_id = $2",
                           [dbClient, jsonBody, chapterId, callback, this](const Result& chapterResult) {
                               if (chapterResult.empty()) {
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Chapter not found"));
                                   resp->setStatusCode(k404NotFound);
                                   callback(resp);
                                   return;
                               }

                               // Строим SQL запрос для обновления
                               string sql = "UPDATE course_chapters SET ";
                               vector<string> updates;
                               vector<string> params;

                               if (jsonBody.isMember("title")) {
                                   updates.push_back("title = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["title"].asString());
                               }

                               if (jsonBody.isMember("description")) {
                                   updates.push_back("description = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["description"].asString());
                               }

                               if (jsonBody.isMember("order")) {
                                   updates.push_back("\"order\" = $" + to_string(params.size() + 1));
                                   params.push_back(to_string(jsonBody["order"].asInt()));
                               }

                               if (updates.empty()) {
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "No fields to update"));
                                   resp->setStatusCode(k400BadRequest);
                                   callback(resp);
                                   return;
                               }

                               sql += std::accumulate(std::next(updates.begin()), updates.end(), updates[0],
                                                      [](const string& a, const string& b) { return a + ", " + b; });

                               sql += " WHERE id = $" + to_string(params.size() + 1);
                               params.push_back(chapterId);

                               // Исправление: передаем параметры по отдельности вместо вектора
                               auto executeUpdate = [sql, params, callback, this](auto&&... args) {
                                   auto dbClient = app().getDbClient();
                                   dbClient->execSqlAsync(sql,
                                                          [callback](const Result& result) {
                                                              Json::Value response;
                                                              response["message"] = "Chapter updated successfully";
                                                              auto resp = HttpResponse::newHttpJsonResponse(response);
                                                              callback(resp);
                                                          },
                                                          [callback, this](const DrogonDbException& e) {
                                                              LOG_ERROR << "Database error updating chapter: " << e.base().what();
                                                              auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Failed to update chapter"));
                                                              resp->setStatusCode(k500InternalServerError);
                                                              callback(resp);
                                                          },
                                                          args...);
                               };

                               // Вызываем с правильным количеством параметров
                               switch (params.size()) {
                               case 1:
                                   executeUpdate(params[0]);
                                   break;
                               case 2:
                                   executeUpdate(params[0], params[1]);
                                   break;
                               case 3:
                                   executeUpdate(params[0], params[1], params[2]);
                                   break;
                               case 4:
                                   executeUpdate(params[0], params[1], params[2], params[3]);
                                   break;
                               default:
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Too many parameters"));
                                   resp->setStatusCode(k400BadRequest);
                                   callback(resp);
                               }
                           },
                           [callback, this](const DrogonDbException& e) {
                               LOG_ERROR << "Database error checking chapter: " << e.base().what();
                               auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                               resp->setStatusCode(k500InternalServerError);
                               callback(resp);
                           },
                           chapterId, courseId
                           );
}

// DELETE /courses/{id}/chapters/{chapterId} - Удалить главу
void CourseController::deleteChapter(const HttpRequestPtr& req,
                                     function<void(const HttpResponsePtr&)>&& callback,
                                     const string& courseId,
                                     const string& chapterId) {

    string userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    if (!isValidUUID(courseId) || !isValidUUID(chapterId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid course or chapter ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Проверяем, что пользователь является автором курса или имеет права администратора
    if (!isCourseAuthor(userId, courseId) && !hasPermission(req, {"основатель", "админ"})) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Forbidden - only course author can delete chapters"));
        resp->setStatusCode(k403Forbidden);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    // Сначала проверяем существование главы
    dbClient->execSqlAsync("SELECT * FROM course_chapters WHERE id = $1 AND course_id = $2",
                           [dbClient, chapterId, callback, this](const Result& chapterResult) {
                               if (chapterResult.empty()) {
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Chapter not found"));
                                   resp->setStatusCode(k404NotFound);
                                   callback(resp);
                                   return;
                               }

                               // Удаляем главу
                               dbClient->execSqlAsync("DELETE FROM course_chapters WHERE id = $1",
                                                      [callback](const Result& result) {
                                                          Json::Value response;
                                                          response["message"] = "Chapter deleted successfully";

                                                          auto resp = HttpResponse::newHttpJsonResponse(response);
                                                          callback(resp);
                                                      },
                                                      [callback, this](const DrogonDbException& e) {
                                                          LOG_ERROR << "Database error deleting chapter: " << e.base().what();
                                                          auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Failed to delete chapter"));
                                                          resp->setStatusCode(k500InternalServerError);
                                                          callback(resp);
                                                      },
                                                      chapterId
                                                      );
                           },
                           [callback, this](const DrogonDbException& e) {
                               LOG_ERROR << "Database error checking chapter: " << e.base().what();
                               auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                               resp->setStatusCode(k500InternalServerError);
                               callback(resp);
                           },
                           chapterId, courseId
                           );
}

// GET /courses/{id}/videos - Получить видео курса
void CourseController::getVideos(const HttpRequestPtr& req,
                                 function<void(const HttpResponsePtr&)>&& callback,
                                 const string& courseId) {

    if (!isValidUUID(courseId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid course ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();
    string userId = getCurrentUserId(req);

    // Получаем роль пользователя для проверки прав доступа
    string userRole = getRoleFromToken(req->getHeader("Authorization").substr(7));
    bool hasAdminAccess = (userRole == "основатель" || userRole == "админ");

    // Проверяем доступ к курсу
    dbClient->execSqlAsync("SELECT * FROM courses WHERE id = $1",
                           [dbClient, userId, hasAdminAccess, callback, courseId, this](const Result& courseResult) {
                               if (courseResult.empty()) {
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Course not found"));
                                   resp->setStatusCode(k404NotFound);
                                   callback(resp);
                                   return;
                               }

                               try {
                                   auto course = Courses(courseResult[0]);

                                   // Проверяем доступ к курсу
                                   bool isPublished = course.getValueOfIsPublished();
                                   bool isPublic = course.getValueOfIsPublic();
                                   bool isAuthor = (userId == course.getValueOfAuthorId());

                                   if (!isPublished && !isAuthor && !hasAdminAccess) {
                                       auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Course not published"));
                                       resp->setStatusCode(k403Forbidden);
                                       callback(resp);
                                       return;
                                   }

                                   if (!isPublic && !isAuthor && !hasAdminAccess && !isEnrolledInCourse(userId, course.getValueOfId())) {
                                       auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Course is private"));
                                       resp->setStatusCode(k403Forbidden);
                                       callback(resp);
                                       return;
                                   }

                                   // Для студентов показываем только одобренные видео, для автора/админа - все
                                   string sql;
                                   if (isAuthor || hasAdminAccess) {
                                       sql = "SELECT * FROM course_videos WHERE course_id = $1 ORDER BY \"order\" ASC";
                                   } else {
                                       sql = "SELECT * FROM course_videos WHERE course_id = $1 AND is_approved = true ORDER BY \"order\" ASC";
                                   }

                                   dbClient->execSqlAsync(
                                       sql,
                                       [callback, this](const Result& videosResult) {
                                           Json::Value response;
                                           Json::Value videosArray(Json::arrayValue);

                                           for (const auto& row : videosResult) {
                                               try {
                                                   auto video = CourseVideos(row);
                                                   videosArray.append(getVideoJsonResponse(video));
                                               } catch (const exception& e) {
                                                   LOG_ERROR << "Error processing video data: " << e.what();
                                               }
                                           }

                                           response["videos"] = videosArray;
                                           auto resp = HttpResponse::newHttpJsonResponse(response);
                                           callback(resp);
                                       },
                                       [callback, this](const DrogonDbException& e) {
                                           LOG_ERROR << "Database error fetching videos: " << e.base().what();
                                           auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                                           resp->setStatusCode(k500InternalServerError);
                                           callback(resp);
                                       },
                                       courseId
                                       );

                               } catch (const exception& e) {
                                   LOG_ERROR << "Error processing course data: " << e.what();
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Error processing course data"));
                                   resp->setStatusCode(k500InternalServerError);
                                   callback(resp);
                               }
                           },
                           [callback, this](const DrogonDbException& e) {
                               LOG_ERROR << "Database error: " << e.base().what();
                               auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                               resp->setStatusCode(k500InternalServerError);
                               callback(resp);
                           },
                           courseId
                           );
}

// Исправленные методы для работы с файлами
bool CourseController::createCourseDirectory(const string& courseId) {
    string coursePath = baseUploadPath_ + "/courses/" + courseId;
    return ensureDirectoryExists(coursePath);
}

bool CourseController::createChapterDirectory(const string& courseId, const string& chapterId) {
    string chapterPath = baseUploadPath_ + "/courses/" + courseId + "/chapters/" + chapterId;
    return ensureDirectoryExists(chapterPath);
}

// Исправленная функция создания директорий
bool CourseController::ensureDirectoryExists(const string& path) {
    try {
        if (fs::exists(path)) {
            return true; // Директория уже существует - это не ошибка
        }
        return fs::create_directories(path);
    } catch (const exception& e) {
        LOG_ERROR << "Error creating directory: " << e.what();
        return false;
    }
}

void CourseController::createVideoInChapter(const HttpRequestPtr& req,
                                            function<void(const HttpResponsePtr&)>&& callback,
                                            const string& courseId,
                                            const string& chapterId) {

    string userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    if (!isValidUUID(courseId) || !isValidUUID(chapterId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid course or chapter ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Проверяем, что пользователь является автором курса или имеет права администратора
    if (!isCourseAuthor(userId, courseId) && !hasPermission(req, {"основатель", "админ"})) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Forbidden - only course author can add videos"));
        resp->setStatusCode(k403Forbidden);
        callback(resp);
        return;
    }

    // Используем MultiPartParser для проверки файлов
    MultiPartParser fileUpload;
    if (fileUpload.parse(req) != 0 || fileUpload.getFiles().size() == 0) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "No video file uploaded or failed to parse request"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    // Сначала проверяем, что глава принадлежит курсу
    dbClient->execSqlAsync("SELECT * FROM course_chapters WHERE id = $1 AND course_id = $2",
                           [dbClient, req, courseId, chapterId, userId, callback, this](const Result& chapterResult) {
                               if (chapterResult.empty()) {
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Chapter not found or doesn't belong to this course"));
                                   resp->setStatusCode(k404NotFound);
                                   callback(resp);
                                   return;
                               }

                               try {
                                   // Создаем директории если не существуют
                                   if (!createCourseDirectory(courseId)) {
                                       throw runtime_error("Failed to create course directory");
                                   }
                                   if (!createChapterDirectory(courseId, chapterId)) {
                                       throw runtime_error("Failed to create chapter directory");
                                   }

                                   // Парсим multipart данные для получения параметров
                                   MultiPartParser fileUpload;
                                   if (fileUpload.parse(req) != 0) {
                                       throw runtime_error("Failed to parse multipart request");
                                   }

                                   auto params = fileUpload.getParameters();

                                   // Сохраняем видео файл
                                   auto videoFileInfo = saveVideoFile(req, courseId, chapterId);
                                   if (videoFileInfo.filename.empty()) {
                                       throw runtime_error("Failed to save video file");
                                   }

                                   // Сохраняем обложку если есть
                                   FileInfo coverFileInfo;
                                   try {
                                       coverFileInfo = saveCoverImage(req, courseId, chapterId);
                                   } catch (const exception& e) {
                                       LOG_WARN << "No cover image uploaded or failed to save: " << e.what();
                                   }

                                   // Получаем данные из параметров multipart
                                   string title = params.find("title") != params.end() ? params.at("title") : "";
                                   string description = params.find("description") != params.end() ? params.at("description") : "";
                                   int order = 0;
                                   try {
                                       string orderStr = params.find("order") != params.end() ? params.at("order") : "0";
                                       order = stoi(orderStr);
                                   } catch (...) {}
                                   int durationSeconds = 0;
                                   try {
                                       string durationStr = params.find("duration_seconds") != params.end() ? params.at("duration_seconds") : "0";
                                       durationSeconds = stoi(durationStr);
                                   } catch (...) {}
                                   string duration = params.find("duration") != params.end() ? params.at("duration") : "00:00";
                                   bool hasSubtitles = params.find("has_subtitles") != params.end() ? params.at("has_subtitles") == "true" : false;
                                   bool hasNotes = params.find("has_notes") != params.end() ? params.at("has_notes") == "true" : false;

                                   if (title.empty()) {
                                       // Удаляем загруженные файлы если данные невалидны
                                       deleteFile(videoFileInfo.full_path);
                                       if (!coverFileInfo.filename.empty()) {
                                           deleteFile(coverFileInfo.full_path);
                                       }
                                       throw runtime_error("Title is required");
                                   }

                                   // Сохраняем в базу данных
                                   string sql = R"(
                    INSERT INTO course_videos
                    (course_id, chapter_id, author_id, title, description, "order",
                     video_filename, video_path, actual_video_path,
                     duration, duration_seconds, cover_path, actual_cover_path,
                     has_subtitles, has_notes, file_size, mime_type)
                    VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17)
                    RETURNING id
                )";

                                   dbClient->execSqlAsync(sql,
                                                          [callback, videoFileInfo](const Result& result) {
                                                              Json::Value response;
                                                              response["id"] = result[0]["id"].as<string>();
                                                              response["message"] = "Video created successfully in chapter";
                                                              response["video_path"] = videoFileInfo.path;
                                                              response["file_size"] = static_cast<Json::Int64>(videoFileInfo.size);

                                                              auto resp = HttpResponse::newHttpJsonResponse(response);
                                                              resp->setStatusCode(k201Created);
                                                              callback(resp);
                                                          },
                                                          [callback, videoFileInfo, coverFileInfo, this](const DrogonDbException& e) {
                                                              // Удаляем файлы если запись в БД не удалась
                                                              deleteFile(videoFileInfo.full_path);
                                                              if (!coverFileInfo.filename.empty()) {
                                                                  deleteFile(coverFileInfo.full_path);
                                                              }

                                                              LOG_ERROR << "Database error creating video in chapter: " << e.base().what();
                                                              auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Failed to create video"));
                                                              resp->setStatusCode(k500InternalServerError);
                                                              callback(resp);
                                                          },
                                                          courseId, chapterId, userId, title, description, order,
                                                          videoFileInfo.filename, videoFileInfo.path, videoFileInfo.full_path,
                                                          duration, durationSeconds,
                                                          coverFileInfo.filename.empty() ? "" : coverFileInfo.path,
                                                          coverFileInfo.filename.empty() ? "" : coverFileInfo.full_path,
                                                          hasSubtitles, hasNotes, static_cast<int64_t>(videoFileInfo.size), videoFileInfo.mime_type
                                                          );

                               } catch (const exception& e) {
                                   LOG_ERROR << "Error creating video in chapter: " << e.what();
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", e.what()));
                                   resp->setStatusCode(k400BadRequest);
                                   callback(resp);
                               }
                           },
                           [callback, this](const DrogonDbException& e) {
                               LOG_ERROR << "Database error checking chapter: " << e.base().what();
                               auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                               resp->setStatusCode(k500InternalServerError);
                               callback(resp);
                           },
                           chapterId, courseId
                           );
}

// PUT /courses/{id}/videos/{videoId} - Обновить видео в курсе
void CourseController::updateVideoInCourse(const HttpRequestPtr& req,
                                           function<void(const HttpResponsePtr&)>&& callback,
                                           const string& courseId,
                                           const string& videoId) {

    string userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    if (!isValidUUID(courseId) || !isValidUUID(videoId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid course or video ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Проверяем, что пользователь является автором видео или имеет права администратора
    if (!isVideoAuthor(userId, videoId) && !hasPermission(req, {"основатель", "админ"})) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Forbidden - only video author can update video"));
        resp->setStatusCode(k403Forbidden);
        callback(resp);
        return;
    }

    Json::Value jsonBody;
    Json::Reader reader;
    string body = string(req->getBody());
    if (!reader.parse(body, jsonBody)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid JSON"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    // Сначала проверяем существование видео
    dbClient->execSqlAsync("SELECT * FROM course_videos WHERE id = $1 AND course_id = $2",
                           [dbClient, jsonBody, videoId, callback, this](const Result& videoResult) {
                               if (videoResult.empty()) {
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Video not found"));
                                   resp->setStatusCode(k404NotFound);
                                   callback(resp);
                                   return;
                               }

                               // Строим SQL запрос для обновления
                               string sql = "UPDATE course_videos SET ";
                               vector<string> updates;
                               vector<string> params;

                               if (jsonBody.isMember("title")) {
                                   updates.push_back("title = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["title"].asString());
                               }

                               if (jsonBody.isMember("description")) {
                                   updates.push_back("description = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["description"].asString());
                               }

                               if (jsonBody.isMember("order")) {
                                   updates.push_back("\"order\" = $" + to_string(params.size() + 1));
                                   params.push_back(to_string(jsonBody["order"].asInt()));
                               }

                               if (jsonBody.isMember("video_filename")) {
                                   updates.push_back("video_filename = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["video_filename"].asString());
                               }

                               if (jsonBody.isMember("video_path")) {
                                   updates.push_back("video_path = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["video_path"].asString());
                               }

                               if (jsonBody.isMember("duration")) {
                                   updates.push_back("duration = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["duration"].asString());
                               }

                               if (jsonBody.isMember("duration_seconds")) {
                                   updates.push_back("duration_seconds = $" + to_string(params.size() + 1));
                                   params.push_back(to_string(jsonBody["duration_seconds"].asInt()));
                               }

                               if (jsonBody.isMember("cover_path")) {
                                   updates.push_back("cover_path = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["cover_path"].asString());
                               }

                               if (jsonBody.isMember("has_subtitles")) {
                                   updates.push_back("has_subtitles = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["has_subtitles"].asBool() ? "true" : "false");
                               }

                               if (jsonBody.isMember("has_notes")) {
                                   updates.push_back("has_notes = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["has_notes"].asBool() ? "true" : "false");
                               }

                               if (jsonBody.isMember("is_approved")) {
                                   // Только админы и основатель могут менять статус одобрения
                                   updates.push_back("is_approved = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["is_approved"].asBool() ? "true" : "false");
                               }

                               if (updates.empty()) {
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "No fields to update"));
                                   resp->setStatusCode(k400BadRequest);
                                   callback(resp);
                                   return;
                               }

                               sql += std::accumulate(std::next(updates.begin()), updates.end(), updates[0],
                                                      [](const string& a, const string& b) { return a + ", " + b; });
                               sql += ", updated_at = CURRENT_TIMESTAMP WHERE id = $" + to_string(params.size() + 1);
                               params.push_back(videoId);

                               // Выполняем обновление
                               auto executeUpdate = [sql, params, callback, this](auto&&... args) {
                                   auto dbClient = app().getDbClient();
                                   dbClient->execSqlAsync(sql,
                                                          [callback](const Result& result) {
                                                              Json::Value response;
                                                              response["message"] = "Video updated successfully";
                                                              auto resp = HttpResponse::newHttpJsonResponse(response);
                                                              callback(resp);
                                                          },
                                                          [callback, this](const DrogonDbException& e) {
                                                              LOG_ERROR << "Database error updating video: " << e.base().what();
                                                              auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Failed to update video"));
                                                              resp->setStatusCode(k500InternalServerError);
                                                              callback(resp);
                                                          },
                                                          args...);
                               };

                               // Вызываем с правильным количеством параметров
                               switch (params.size()) {
                               case 1: executeUpdate(params[0]); break;
                               case 2: executeUpdate(params[0], params[1]); break;
                               case 3: executeUpdate(params[0], params[1], params[2]); break;
                               case 4: executeUpdate(params[0], params[1], params[2], params[3]); break;
                               case 5: executeUpdate(params[0], params[1], params[2], params[3], params[4]); break;
                               case 6: executeUpdate(params[0], params[1], params[2], params[3], params[4], params[5]); break;
                               case 7: executeUpdate(params[0], params[1], params[2], params[3], params[4], params[5], params[6]); break;
                               case 8: executeUpdate(params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7]); break;
                               case 9: executeUpdate(params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7], params[8]); break;
                               case 10: executeUpdate(params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7], params[8], params[9]); break;
                               case 11: executeUpdate(params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7], params[8], params[9], params[10]); break;
                               case 12: executeUpdate(params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7], params[8], params[9], params[10], params[11]); break;
                               default:
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Too many parameters"));
                                   resp->setStatusCode(k400BadRequest);
                                   callback(resp);
                               }
                           },
                           [callback, this](const DrogonDbException& e) {
                               LOG_ERROR << "Database error checking video: " << e.base().what();
                               auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                               resp->setStatusCode(k500InternalServerError);
                               callback(resp);
                           },
                           videoId, courseId
                           );
}

// PUT /courses/{id}/chapters/{chapterId}/videos/{videoId} - Обновить видео в главе
void CourseController::updateVideoInChapter(const HttpRequestPtr& req,
                                            function<void(const HttpResponsePtr&)>&& callback,
                                            const string& courseId,
                                            const string& chapterId,
                                            const string& videoId) {

    // Используем ту же логику, что и для updateVideoInCourse, но с дополнительной проверкой chapterId
    string userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    if (!isValidUUID(courseId) || !isValidUUID(chapterId) || !isValidUUID(videoId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid course, chapter or video ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Проверяем, что пользователь является автором видео или имеет права администратора
    if (!isVideoAuthor(userId, videoId) && !hasPermission(req, {"основатель", "админ"})) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Forbidden - only video author can update video"));
        resp->setStatusCode(k403Forbidden);
        callback(resp);
        return;
    }

    Json::Value jsonBody;
    Json::Reader reader;
    string body = string(req->getBody());
    if (!reader.parse(body, jsonBody)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid JSON"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    // Сначала проверяем существование видео и принадлежность к главе и курсу
    dbClient->execSqlAsync("SELECT * FROM course_videos WHERE id = $1 AND course_id = $2 AND chapter_id = $3",
                           [dbClient, jsonBody, videoId, callback, this](const Result& videoResult) {
                               if (videoResult.empty()) {
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Video not found in specified chapter and course"));
                                   resp->setStatusCode(k404NotFound);
                                   callback(resp);
                                   return;
                               }

                               // Строим SQL запрос для обновления (аналогично updateVideoInCourse)
                               string sql = "UPDATE course_videos SET ";
                               vector<string> updates;
                               vector<string> params;

                               if (jsonBody.isMember("title")) {
                                   updates.push_back("title = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["title"].asString());
                               }

                               if (jsonBody.isMember("description")) {
                                   updates.push_back("description = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["description"].asString());
                               }

                               if (jsonBody.isMember("order")) {
                                   updates.push_back("\"order\" = $" + to_string(params.size() + 1));
                                   params.push_back(to_string(jsonBody["order"].asInt()));
                               }

                               if (jsonBody.isMember("video_filename")) {
                                   updates.push_back("video_filename = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["video_filename"].asString());
                               }

                               if (jsonBody.isMember("video_path")) {
                                   updates.push_back("video_path = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["video_path"].asString());
                               }

                               if (jsonBody.isMember("duration")) {
                                   updates.push_back("duration = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["duration"].asString());
                               }

                               if (jsonBody.isMember("duration_seconds")) {
                                   updates.push_back("duration_seconds = $" + to_string(params.size() + 1));
                                   params.push_back(to_string(jsonBody["duration_seconds"].asInt()));
                               }

                               if (jsonBody.isMember("cover_path")) {
                                   updates.push_back("cover_path = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["cover_path"].asString());
                               }

                               if (jsonBody.isMember("has_subtitles")) {
                                   updates.push_back("has_subtitles = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["has_subtitles"].asBool() ? "true" : "false");
                               }

                               if (jsonBody.isMember("has_notes")) {
                                   updates.push_back("has_notes = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["has_notes"].asBool() ? "true" : "false");
                               }

                               if (jsonBody.isMember("is_approved")) {
                                   // Только админы и основатель могут менять статус одобрения
                                   updates.push_back("is_approved = $" + to_string(params.size() + 1));
                                   params.push_back(jsonBody["is_approved"].asBool() ? "true" : "false");
                               }

                               if (updates.empty()) {
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "No fields to update"));
                                   resp->setStatusCode(k400BadRequest);
                                   callback(resp);
                                   return;
                               }

                               sql += std::accumulate(std::next(updates.begin()), updates.end(), updates[0],
                                                      [](const string& a, const string& b) { return a + ", " + b; });
                               sql += ", updated_at = CURRENT_TIMESTAMP WHERE id = $" + to_string(params.size() + 1);
                               params.push_back(videoId);

                               // Выполняем обновление
                               auto executeUpdate = [sql, params, callback, this](auto&&... args) {
                                   auto dbClient = app().getDbClient();
                                   dbClient->execSqlAsync(sql,
                                                          [callback](const Result& result) {
                                                              Json::Value response;
                                                              response["message"] = "Video updated successfully";
                                                              auto resp = HttpResponse::newHttpJsonResponse(response);
                                                              callback(resp);
                                                          },
                                                          [callback, this](const DrogonDbException& e) {
                                                              LOG_ERROR << "Database error updating video: " << e.base().what();
                                                              auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Failed to update video"));
                                                              resp->setStatusCode(k500InternalServerError);
                                                              callback(resp);
                                                          },
                                                          args...);
                               };

                               // Вызываем с правильным количеством параметров
                               switch (params.size()) {
                               case 1: executeUpdate(params[0]); break;
                               case 2: executeUpdate(params[0], params[1]); break;
                               case 3: executeUpdate(params[0], params[1], params[2]); break;
                               case 4: executeUpdate(params[0], params[1], params[2], params[3]); break;
                               case 5: executeUpdate(params[0], params[1], params[2], params[3], params[4]); break;
                               case 6: executeUpdate(params[0], params[1], params[2], params[3], params[4], params[5]); break;
                               case 7: executeUpdate(params[0], params[1], params[2], params[3], params[4], params[5], params[6]); break;
                               case 8: executeUpdate(params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7]); break;
                               case 9: executeUpdate(params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7], params[8]); break;
                               case 10: executeUpdate(params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7], params[8], params[9]); break;
                               case 11: executeUpdate(params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7], params[8], params[9], params[10]); break;
                               case 12: executeUpdate(params[0], params[1], params[2], params[3], params[4], params[5], params[6], params[7], params[8], params[9], params[10], params[11]); break;
                               default:
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Too many parameters"));
                                   resp->setStatusCode(k400BadRequest);
                                   callback(resp);
                               }
                           },
                           [callback, this](const DrogonDbException& e) {
                               LOG_ERROR << "Database error checking video: " << e.base().what();
                               auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                               resp->setStatusCode(k500InternalServerError);
                               callback(resp);
                           },
                           videoId, courseId, chapterId
                           );
}

// DELETE /courses/{id}/videos/{videoId} - Удалить видео
void CourseController::deleteVideo(const HttpRequestPtr& req,
                                   function<void(const HttpResponsePtr&)>&& callback,
                                   const string& courseId,
                                   const string& videoId) {

    string userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    if (!isValidUUID(courseId) || !isValidUUID(videoId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid course or video ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Проверяем, что пользователь является автором видео или имеет права администратора
    if (!isVideoAuthor(userId, videoId) && !hasPermission(req, {"основатель", "админ"})) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Forbidden - only video author can delete video"));
        resp->setStatusCode(k403Forbidden);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    // Сначала проверяем существование видео
    dbClient->execSqlAsync("SELECT * FROM course_videos WHERE id = $1 AND course_id = $2",
                           [dbClient, videoId, callback, this](const Result& videoResult) {
                               if (videoResult.empty()) {
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Video not found"));
                                   resp->setStatusCode(k404NotFound);
                                   callback(resp);
                                   return;
                               }

                               // Получаем информацию о файле для удаления
                               try {
                                   auto video = CourseVideos(videoResult[0]);
                                   // ИСПРАВЛЕНО: используем существующие поля вместо getValueOfActualVideoPath и getValueOfActualCoverPath
                                   string actualVideoPath = video.getValueOfVideoPath(); // Используем video_path вместо actual_video_path
                                   string actualCoverPath = video.getValueOfCoverPath(); // Используем cover_path вместо actual_cover_path

                                   // Удаляем видео
                                   dbClient->execSqlAsync("DELETE FROM course_videos WHERE id = $1",
                                                          [callback, actualVideoPath, actualCoverPath, this](const Result& result) {
                                                              // Удаляем физические файлы
                                                              if (!actualVideoPath.empty()) {
                                                                  deleteFile(actualVideoPath);
                                                              }
                                                              if (!actualCoverPath.empty()) {
                                                                  deleteFile(actualCoverPath);
                                                              }

                                                              Json::Value response;
                                                              response["message"] = "Video deleted successfully";

                                                              auto resp = HttpResponse::newHttpJsonResponse(response);
                                                              callback(resp);
                                                          },
                                                          [callback, this](const DrogonDbException& e) {
                                                              LOG_ERROR << "Database error deleting video: " << e.base().what();
                                                              auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Failed to delete video"));
                                                              resp->setStatusCode(k500InternalServerError);
                                                              callback(resp);
                                                          },
                                                          videoId
                                                          );
                               } catch (const exception& e) {
                                   LOG_ERROR << "Error getting video info: " << e.what();
                                   // Все равно пытаемся удалить запись из БД
                                   dbClient->execSqlAsync("DELETE FROM course_videos WHERE id = $1",
                                                          [callback](const Result& result) {
                                                              Json::Value response;
                                                              response["message"] = "Video deleted successfully";

                                                              auto resp = HttpResponse::newHttpJsonResponse(response);
                                                              callback(resp);
                                                          },
                                                          [callback, this](const DrogonDbException& e) {
                                                              LOG_ERROR << "Database error deleting video: " << e.base().what();
                                                              auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Failed to delete video"));
                                                              resp->setStatusCode(k500InternalServerError);
                                                              callback(resp);
                                                          },
                                                          videoId
                                                          );
                               }
                           },
                           [callback, this](const DrogonDbException& e) {
                               LOG_ERROR << "Database error checking video: " << e.base().what();
                               auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                               resp->setStatusCode(k500InternalServerError);
                               callback(resp);
                           },
                           videoId, courseId
                           );
}

// PUT /courses/{id}/videos/{videoId}/position - Переместить видео между главами или изменить порядок
void CourseController::moveVideoPosition(const HttpRequestPtr& req,
                                         function<void(const HttpResponsePtr&)>&& callback,
                                         const string& courseId,
                                         const string& videoId) {

    string userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    if (!isValidUUID(courseId) || !isValidUUID(videoId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid course or video ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Проверяем, что пользователь является автором курса или имеет права администратора
    if (!isCourseAuthor(userId, courseId) && !hasPermission(req, {"основатель", "админ"})) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Forbidden - only course author can move videos"));
        resp->setStatusCode(k403Forbidden);
        callback(resp);
        return;
    }

    Json::Value jsonBody;
    Json::Reader reader;
    string body = string(req->getBody());
    if (!reader.parse(body, jsonBody)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid JSON"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    if (!jsonBody.isMember("order") || !jsonBody.isMember("chapter_id")) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Missing required fields: order, chapter_id"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    int order = jsonBody["order"].asInt();
    string newChapterId = jsonBody["chapter_id"].asString();

    // Если chapter_id пустой или null, перемещаем видео в корень курса
    if (newChapterId.empty() || newChapterId == "null") {
        newChapterId = "";
    } else if (!isValidUUID(newChapterId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid chapter ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Проверяем существование видео
    dbClient->execSqlAsync("SELECT * FROM course_videos WHERE id = $1 AND course_id = $2",
                           [dbClient, videoId, courseId, newChapterId, order, callback, this](const Result& videoResult) {
                               if (videoResult.empty()) {
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Video not found"));
                                   resp->setStatusCode(k404NotFound);
                                   callback(resp);
                                   return;
                               }

                               // Если указана новая глава, проверяем ее существование и принадлежность курсу
                               if (!newChapterId.empty()) {
                                   dbClient->execSqlAsync("SELECT * FROM course_chapters WHERE id = $1 AND course_id = $2",
                                                          [dbClient, videoId, courseId, newChapterId, order, callback, this](const Result& chapterResult) {
                                                              if (chapterResult.empty()) {
                                                                  auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Chapter not found or doesn't belong to this course"));
                                                                  resp->setStatusCode(k404NotFound);
                                                                  callback(resp);
                                                                  return;
                                                              }

                                                              // Обновляем позицию видео
                                                              updateVideoPosition(dbClient, videoId, courseId, newChapterId, order, callback);
                                                          },
                                                          [callback, this](const DrogonDbException& e) {
                                                              LOG_ERROR << "Database error checking chapter: " << e.base().what();
                                                              auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                                                              resp->setStatusCode(k500InternalServerError);
                                                              callback(resp);
                                                          },
                                                          newChapterId, courseId
                                                          );
                               } else {
                                   // Перемещаем видео в корень курса (без главы)
                                   updateVideoPosition(dbClient, videoId, courseId, newChapterId, order, callback);
                               }
                           },
                           [callback, this](const DrogonDbException& e) {
                               LOG_ERROR << "Database error checking video: " << e.base().what();
                               auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                               resp->setStatusCode(k500InternalServerError);
                               callback(resp);
                           },
                           videoId, courseId
                           );
}

// Вспомогательный метод для обновления позиции видео
void CourseController::updateVideoPosition(const orm::DbClientPtr& dbClient,
                                           const string& videoId,
                                           const string& courseId,
                                           const string& chapterId,
                                           int order,
                                           const function<void(const HttpResponsePtr&)>& callback) {

    string sql;
    if (chapterId.empty()) {
        sql = "UPDATE course_videos SET chapter_id = NULL, \"order\" = $1 WHERE id = $2 AND course_id = $3";
    } else {
        sql = "UPDATE course_videos SET chapter_id = $1, \"order\" = $2 WHERE id = $3 AND course_id = $4";
    }

    auto executeUpdate = [sql, videoId, courseId, chapterId, order, callback, this](auto&&... args) {
        auto dbClient = app().getDbClient();
        dbClient->execSqlAsync(sql,
                               [callback](const Result& result) {
                                   Json::Value response;
                                   response["message"] = "Video position updated successfully";
                                   auto resp = HttpResponse::newHttpJsonResponse(response);
                                   callback(resp);
                               },
                               [callback, this](const DrogonDbException& e) {
                                   LOG_ERROR << "Database error updating video position: " << e.base().what();
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Failed to update video position"));
                                   resp->setStatusCode(k500InternalServerError);
                                   callback(resp);
                               },
                               args...);
    };

    if (chapterId.empty()) {
        executeUpdate(order, videoId, courseId);
    } else {
        executeUpdate(chapterId, order, videoId, courseId);
    }
}

// GET /courses/{id}/enrollments - Получить список записавшихся
void CourseController::getEnrollments(const HttpRequestPtr& req,
                                      function<void(const HttpResponsePtr&)>&& callback,
                                      const string& courseId) {

    string userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    if (!isValidUUID(courseId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid course ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    // Проверяем, что пользователь является автором курса или имеет права администратора
    if (!isCourseAuthor(userId, courseId) && !hasPermission(req, {"основатель", "админ"})) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Forbidden - only course author can view enrollments"));
        resp->setStatusCode(k403Forbidden);
        callback(resp);
        return;
    }

    // Получаем список записавшихся с информацией о пользователях
    dbClient->execSqlAsync(
        "SELECT ce.*, u.username, u.email, u.avatar_path "
        "FROM course_enrollments ce "
        "JOIN users u ON ce.user_id = u.id "
        "WHERE ce.course_id = $1 "
        "ORDER BY ce.enrolled_at DESC",
        [callback](const Result& result) {
            Json::Value response;
            Json::Value enrollmentsArray(Json::arrayValue);

            for (const auto& row : result) {
                Json::Value enrollment;
                enrollment["enrollment_id"] = row["id"].as<string>();
                enrollment["user_id"] = row["user_id"].as<string>();
                enrollment["username"] = row["username"].as<string>();
                enrollment["email"] = row["email"].as<string>();
                enrollment["completion_percentage"] = row["completion_percentage"].as<int>();
                enrollment["is_completed"] = row["is_completed"].as<bool>();
                enrollment["enrolled_at"] = row["enrolled_at"].as<string>();
                enrollment["last_accessed_at"] = row["last_accessed_at"].as<string>();

                if (!row["avatar_path"].isNull()) {
                    enrollment["avatar_path"] = row["avatar_path"].as<string>();
                } else {
                    enrollment["avatar_path"] = Json::nullValue;
                }

                enrollmentsArray.append(enrollment);
            }

            response["enrollments"] = enrollmentsArray;
            response["total"] = static_cast<int>(result.size());

            auto resp = HttpResponse::newHttpJsonResponse(response);
            callback(resp);
        },
        [callback, this](const DrogonDbException& e) {
            LOG_ERROR << "Database error fetching enrollments: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        courseId
        );
}

// DELETE /courses/{id}/enroll - Отписаться от курса
void CourseController::unenrollFromCourse(const HttpRequestPtr& req,
                                          function<void(const HttpResponsePtr&)>&& callback,
                                          const string& courseId) {

    string userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    if (!isValidUUID(courseId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid course ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    // Удаляем запись о зачислении
    dbClient->execSqlAsync("DELETE FROM course_enrollments WHERE user_id = $1 AND course_id = $2",
                           [callback](const Result& result) {
                               Json::Value response;
                               if (result.affectedRows() > 0) {
                                   response["message"] = "Successfully unenrolled from course";
                               } else {
                                   response["message"] = "Not enrolled in this course";
                               }

                               auto resp = HttpResponse::newHttpJsonResponse(response);
                               callback(resp);
                           },
                           [callback, this](const DrogonDbException& e) {
                               LOG_ERROR << "Database error unenrolling from course: " << e.base().what();
                               auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Failed to unenroll from course"));
                               resp->setStatusCode(k500InternalServerError);
                               callback(resp);
                           },
                           userId, courseId
                           );
}

void CourseController::uploadVideoFile(const HttpRequestPtr& req,
                                       function<void(const HttpResponsePtr&)>&& callback,
                                       const string& courseId) {

    string userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    if (!isValidUUID(courseId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid course ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Используем MultiPartParser для проверки файлов
    MultiPartParser fileUpload;
    if (fileUpload.parse(req) != 0 || fileUpload.getFiles().size() == 0) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "No file uploaded or failed to parse request"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    try {
        // Создаем директорию курса если не существует
        if (!createCourseDirectory(courseId)) {
            throw runtime_error("Failed to create course directory");
        }

        // Сохраняем файл
        auto fileInfo = saveVideoFile(req, courseId);
        if (fileInfo.filename.empty()) {
            throw runtime_error("Failed to save file");
        }

        Json::Value response;
        response["filename"] = fileInfo.filename;
        response["path"] = fileInfo.path;
        response["full_path"] = fileInfo.full_path;
        response["size"] = static_cast<Json::Int64>(fileInfo.size);
        response["mime_type"] = fileInfo.mime_type;
        response["message"] = "File uploaded successfully";

        auto resp = HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const exception& e) {
        LOG_ERROR << "Error uploading file: " << e.what();
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", e.what()));
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
    }
}

void CourseController::createVideoInCourse(const HttpRequestPtr& req,
                                           function<void(const HttpResponsePtr&)>&& callback,
                                           const string& courseId) {

    string userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    if (!isValidUUID(courseId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid course ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Проверяем, что пользователь является автором курса или имеет права администратора
    if (!isCourseAuthor(userId, courseId) && !hasPermission(req, {"основатель", "админ"})) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Forbidden - only course author can add videos"));
        resp->setStatusCode(k403Forbidden);
        callback(resp);
        return;
    }

    // Используем MultiPartParser для парсинга multipart/form-data
    MultiPartParser fileUpload;
    if (fileUpload.parse(req) != 0) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Failed to parse multipart request"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Проверяем, есть ли видео файл
    auto files = fileUpload.getFiles();
    if (files.size() == 0) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "No video file uploaded"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    try {
        // Создаем директорию курса если не существует
        string videosPath = baseUploadPath_ + "/courses/" + courseId + "/videos";
        if (!ensureDirectoryExists(videosPath)) {
            throw runtime_error("Failed to create videos directory");
        }

        // Сохраняем видео файл
        auto videoFileInfo = saveVideoFile(req, courseId);
        if (videoFileInfo.filename.empty()) {
            throw runtime_error("Failed to save video file");
        }

        // Сохраняем обложку если есть
        FileInfo coverFileInfo;
        try {
            coverFileInfo = saveCoverImage(req, courseId);
        } catch (const exception& e) {
            LOG_WARN << "No cover image uploaded or failed to save: " << e.what();
        }

        // Получаем параметры из multipart данных
        auto params = fileUpload.getParameters();

        // Извлекаем обязательные поля из form-data
        string title;
        if (params.find("title") != params.end()) {
            title = params.at("title");
        } else {
            // Удаляем загруженные файлы если данные невалидны
            deleteFile(videoFileInfo.full_path);
            if (!coverFileInfo.filename.empty()) {
                deleteFile(coverFileInfo.full_path);
            }
            throw runtime_error("Title is required");
        }

        string description = params.find("description") != params.end() ? params.at("description") : "";

        int order = 0;
        if (params.find("order") != params.end()) {
            try {
                order = stoi(params.at("order"));
            } catch (const exception& e) {
                LOG_WARN << "Invalid order value, using default: " << e.what();
                order = 0;
            }
        }

        int durationSeconds = 0;
        if (params.find("duration_seconds") != params.end()) {
            try {
                durationSeconds = stoi(params.at("duration_seconds"));
            } catch (const exception& e) {
                LOG_WARN << "Invalid duration_seconds value: " << e.what();
            }
        }

        string duration = params.find("duration") != params.end() ? params.at("duration") : "00:00";
        bool hasSubtitles = params.find("has_subtitles") != params.end() ?
                                (params.at("has_subtitles") == "true" || params.at("has_subtitles") == "1") : false;
        bool hasNotes = params.find("has_notes") != params.end() ?
                            (params.at("has_notes") == "true" || params.at("has_notes") == "1") : false;

        // Сохраняем в базу данных
        string sql = R"(
            INSERT INTO course_videos
            (course_id, author_id, title, description, "order",
             video_filename, video_path, actual_video_path,
             duration, duration_seconds, cover_path, actual_cover_path,
             has_subtitles, has_notes, file_size, mime_type)
            VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16)
            RETURNING id
        )";

        dbClient->execSqlAsync(sql,
                               [callback, videoFileInfo](const Result& result) {
                                   Json::Value response;
                                   response["id"] = result[0]["id"].as<string>();
                                   response["message"] = "Video created successfully";
                                   response["video_path"] = videoFileInfo.path;
                                   response["file_size"] = static_cast<Json::Int64>(videoFileInfo.size);

                                   auto resp = HttpResponse::newHttpJsonResponse(response);
                                   resp->setStatusCode(k201Created);
                                   callback(resp);
                               },
                               [callback, videoFileInfo, coverFileInfo, this](const DrogonDbException& e) {
                                   // Удаляем файлы если запись в БД не удалась
                                   deleteFile(videoFileInfo.full_path);
                                   if (!coverFileInfo.filename.empty()) {
                                       deleteFile(coverFileInfo.full_path);
                                   }

                                   LOG_ERROR << "Database error creating video: " << e.base().what();
                                   auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Failed to create video"));
                                   resp->setStatusCode(k500InternalServerError);
                                   callback(resp);
                               },
                               courseId, userId, title, description, order,
                               videoFileInfo.filename, videoFileInfo.path, videoFileInfo.full_path,
                               duration, durationSeconds,
                               coverFileInfo.filename.empty() ? "" : coverFileInfo.path,
                               coverFileInfo.filename.empty() ? "" : coverFileInfo.full_path,
                               hasSubtitles, hasNotes, static_cast<int64_t>(videoFileInfo.size), videoFileInfo.mime_type
                               );

    } catch (const exception& e) {
        LOG_ERROR << "Error creating video: " << e.what();
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", e.what()));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
    }
}

CourseController::FileInfo CourseController::saveVideoFile(const HttpRequestPtr& req,
                                                           const string& courseId,
                                                           const string& chapterId) {
    FileInfo fileInfo;

    // Используем MultiPartParser для получения файлов
    MultiPartParser fileUpload;
    if (fileUpload.parse(req) != 0 || fileUpload.getFiles().size() == 0) {
        return fileInfo; // возвращаем пустой FileInfo
    }

    auto files = fileUpload.getFiles();

    // Ищем видео файл (первый файл или с определенным именем)
    for (const auto& file : files) {
        string name = file.getFileName();

        // Получаем MIME тип из имени файла, а не из ContentType
        string mimeType = getMimeType(name);

        // Проверяем, что это видео файл по расширению
        if (mimeType.find("video/") == 0 ||
            name.find(".mp4") != string::npos ||
            name.find(".avi") != string::npos ||
            name.find(".mov") != string::npos ||
            name.find(".webm") != string::npos) {

            // Определяем путь для сохранения
            string savePath;
            if (chapterId.empty()) {
                savePath = baseUploadPath_ + "/courses/" + courseId + "/videos";
            } else {
                savePath = baseUploadPath_ + "/courses/" + courseId + "/chapters/" + chapterId + "/videos";
            }

            // Создаем директорию если не существует
            if (!ensureDirectoryExists(savePath)) {
                throw runtime_error("Failed to create directory: " + savePath);
            }

            string filename = generateFilename(file.getFileName());
            string fullPath = savePath + "/" + filename;

            // Сохраняем файл
            file.saveAs(fullPath);

            // Заполняем информацию о файле
            fileInfo.filename = filename;
            fileInfo.path = fullPath.substr(baseUploadPath_.length() + 1); // относительный путь
            fileInfo.full_path = fullPath;
            fileInfo.size = file.fileLength();
            fileInfo.mime_type = mimeType;

            break; // Используем первый найденный видео файл
        }
    }

    return fileInfo;
}

// =============================================================================
// ИСПРАВЛЕННЫЕ МЕТОДЫ ДЛЯ ОБНОВЛЕНИЯ ПОРЯДКА
// =============================================================================

void CourseController::updateChapterVideosOrder(const HttpRequestPtr& req,
                                                function<void(const HttpResponsePtr&)>&& callback,
                                                const string& courseId,
                                                const string& chapterId) {

    string userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    if (!isValidUUID(courseId) || !isValidUUID(chapterId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid course or chapter ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Проверяем, что пользователь является автором курса или имеет права администратора
    if (!isCourseAuthor(userId, courseId) && !hasPermission(req, {"основатель", "админ"})) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Forbidden - only course author can modify video order"));
        resp->setStatusCode(k403Forbidden);
        callback(resp);
        return;
    }

    Json::Value jsonBody;
    Json::Reader reader;
    string body = string(req->getBody());
    if (!reader.parse(body, jsonBody)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid JSON"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    if (!jsonBody.isMember("video_order") || !jsonBody["video_order"].isArray()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Missing or invalid video_order array"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    const Json::Value& videoOrder = jsonBody["video_order"];
    if (videoOrder.size() == 0) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Empty video_order array"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Проверяем валидность всех UUID в массиве
    for (Json::ArrayIndex i = 0; i < videoOrder.size(); ++i) {
        string videoId = videoOrder[i].asString();
        if (!isValidUUID(videoId)) {
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid video ID format in video_order array"));
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            return;
        }
    }

    auto dbClient = app().getDbClient();
    auto callbackPtr = std::make_shared<std::function<void(const HttpResponsePtr&)>>(std::move(callback));

    // Используем транзакцию для последовательного обновления
    dbClient->newTransactionAsync([dbClient, videoOrder, courseId, chapterId, callbackPtr, this](const std::shared_ptr<drogon::orm::Transaction>& transPtr) {
        auto remainingUpdates = std::make_shared<int>(videoOrder.size());
        auto hasError = std::make_shared<bool>(false);

        for (Json::ArrayIndex i = 0; i < videoOrder.size(); ++i) {
            string videoId = videoOrder[i].asString();
            int order = i + 1;

            transPtr->execSqlAsync(
                "UPDATE course_videos SET \"order\" = $1 WHERE id = $2 AND course_id = $3 AND chapter_id = $4",
                [remainingUpdates, callbackPtr, transPtr, hasError](const Result& result) {
                    (*remainingUpdates)--;
                    if (*remainingUpdates == 0 && !*hasError) {
                        // Все обновления завершены
                        Json::Value response;
                        response["message"] = "Video order updated successfully";
                        auto resp = HttpResponse::newHttpJsonResponse(response);
                        (*callbackPtr)(resp);
                    }
                },
                [callbackPtr, hasError, this](const DrogonDbException& e) {
                    // При ошибке транзакция автоматически откатывается
                    *hasError = true;
                    LOG_ERROR << "Database error updating video order: " << e.base().what();
                    auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Failed to update video order"));
                    resp->setStatusCode(k500InternalServerError);
                    (*callbackPtr)(resp);
                },
                order, videoId, courseId, chapterId
                );
        }
    });
}

void CourseController::updateVideosOrder(const HttpRequestPtr& req,
                                         function<void(const HttpResponsePtr&)>&& callback,
                                         const string& courseId) {

    string userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    if (!isValidUUID(courseId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid course ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Проверяем, что пользователь является автором курса или имеет права администратора
    if (!isCourseAuthor(userId, courseId) && !hasPermission(req, {"основатель", "админ"})) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Forbidden - only course author can modify video order"));
        resp->setStatusCode(k403Forbidden);
        callback(resp);
        return;
    }

    Json::Value jsonBody;
    Json::Reader reader;
    string body = string(req->getBody());
    if (!reader.parse(body, jsonBody)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid JSON"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    if (!jsonBody.isMember("video_order") || !jsonBody["video_order"].isArray()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Missing or invalid video_order array"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    const Json::Value& videoOrder = jsonBody["video_order"];
    if (videoOrder.size() == 0) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Empty video_order array"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Проверяем валидность всех UUID в массиве
    for (Json::ArrayIndex i = 0; i < videoOrder.size(); ++i) {
        string videoId = videoOrder[i].asString();
        if (!isValidUUID(videoId)) {
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid video ID format in video_order array"));
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            return;
        }
    }

    auto dbClient = app().getDbClient();
    auto callbackPtr = std::make_shared<std::function<void(const HttpResponsePtr&)>>(std::move(callback));

    // Используем транзакцию для последовательного обновления
    dbClient->newTransactionAsync([dbClient, videoOrder, courseId, callbackPtr, this](const std::shared_ptr<drogon::orm::Transaction>& transPtr) {
        auto remainingUpdates = std::make_shared<int>(videoOrder.size());
        auto hasError = std::make_shared<bool>(false);

        for (Json::ArrayIndex i = 0; i < videoOrder.size(); ++i) {
            string videoId = videoOrder[i].asString();
            int order = i + 1;

            transPtr->execSqlAsync(
                "UPDATE course_videos SET \"order\" = $1 WHERE id = $2 AND course_id = $3 AND chapter_id IS NULL",
                [remainingUpdates, callbackPtr, transPtr, hasError](const Result& result) {
                    (*remainingUpdates)--;
                    if (*remainingUpdates == 0 && !*hasError) {
                        // Все обновления завершены
                        Json::Value response;
                        response["message"] = "Video order updated successfully";
                        auto resp = HttpResponse::newHttpJsonResponse(response);
                        (*callbackPtr)(resp);
                    }
                },
                [callbackPtr, hasError, this](const DrogonDbException& e) {
                    // При ошибке транзакция автоматически откатывается
                    *hasError = true;
                    LOG_ERROR << "Database error updating video order: " << e.base().what();
                    auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Failed to update video order"));
                    resp->setStatusCode(k500InternalServerError);
                    (*callbackPtr)(resp);
                },
                order, videoId, courseId
                );
        }
    });
}

void CourseController::updateChaptersOrder(const HttpRequestPtr& req,
                                           function<void(const HttpResponsePtr&)>&& callback,
                                           const string& courseId) {

    string userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    if (!isValidUUID(courseId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid course ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Проверяем, что пользователь является автором курса или имеет права администратора
    if (!isCourseAuthor(userId, courseId) && !hasPermission(req, {"основатель", "админ"})) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Forbidden - only course author can modify chapter order"));
        resp->setStatusCode(k403Forbidden);
        callback(resp);
        return;
    }

    Json::Value jsonBody;
    Json::Reader reader;
    string body = string(req->getBody());
    if (!reader.parse(body, jsonBody)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid JSON"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    if (!jsonBody.isMember("chapter_order") || !jsonBody["chapter_order"].isArray()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Missing or invalid chapter_order array"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    const Json::Value& chapterOrder = jsonBody["chapter_order"];
    if (chapterOrder.size() == 0) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Empty chapter_order array"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();
    auto callbackPtr = std::make_shared<std::function<void(const HttpResponsePtr&)>>(std::move(callback));

    // Используем транзакцию для последовательного обновления
    dbClient->newTransactionAsync([dbClient, chapterOrder, courseId, callbackPtr, this](const std::shared_ptr<drogon::orm::Transaction>& transPtr) {
        auto remainingUpdates = std::make_shared<int>(chapterOrder.size());
        auto hasError = std::make_shared<bool>(false);

        for (Json::ArrayIndex i = 0; i < chapterOrder.size(); ++i) {
            string chapterId = chapterOrder[i].asString();
            if (!isValidUUID(chapterId)) {
                // Пропускаем невалидные ID, но это не должно происходить из-за предыдущих проверок
                (*remainingUpdates)--;
                if (*remainingUpdates == 0 && !*hasError) {
                    Json::Value response;
                    response["message"] = "Chapter order updated successfully";
                    auto resp = HttpResponse::newHttpJsonResponse(response);
                    (*callbackPtr)(resp);
                }
                continue;
            }
            int order = i + 1;

            transPtr->execSqlAsync(
                "UPDATE course_chapters SET \"order\" = $1 WHERE id = $2 AND course_id = $3",
                [remainingUpdates, callbackPtr, transPtr, hasError](const Result& result) {
                    (*remainingUpdates)--;
                    if (*remainingUpdates == 0 && !*hasError) {
                        // Все обновления завершены
                        Json::Value response;
                        response["message"] = "Chapter order updated successfully";
                        auto resp = HttpResponse::newHttpJsonResponse(response);
                        (*callbackPtr)(resp);
                    }
                },
                [callbackPtr, hasError, this](const DrogonDbException& e) {
                    // При ошибке транзакция автоматически откатывается
                    *hasError = true;
                    LOG_ERROR << "Database error updating chapter order: " << e.base().what();
                    auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Failed to update chapter order"));
                    resp->setStatusCode(k500InternalServerError);
                    (*callbackPtr)(resp);
                },
                order, chapterId, courseId
                );
        }
    });
}
