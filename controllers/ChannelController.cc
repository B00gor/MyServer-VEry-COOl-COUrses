#include "ChannelController.h"
#include <drogon/HttpResponse.h>
#include <drogon/utils/Utilities.h>
#include <drogon/drogon.h>
#include <regex>
#include <algorithm>

using namespace drogon;
using namespace drogon::orm;
using namespace std;

// Вспомогательная функция для создания JSON ответов
Json::Value ChannelController::createJsonResponse(const string& key, const string& value) {
    Json::Value json;
    json[key] = value;
    return json;
}

// Валидация UUID
bool ChannelController::isValidUUID(const string& uuid) {
    regex uuidPattern("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$");
    return regex_match(uuid, uuidPattern);
}

// JWT аутентификация
string ChannelController::getCurrentUserId(const HttpRequestPtr& req) {
    auto authHeader = req->getHeader("Authorization");
    if (authHeader.empty() || authHeader.find("Bearer ") != 0) {
        return "";
    }

    string token = authHeader.substr(7);
    return getUserIdFromToken(token);
}

bool ChannelController::validateJWT(const string& token) {
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

string ChannelController::getUserIdFromToken(const string& token) {
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

string ChannelController::getRoleFromToken(const string& token) {
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

bool ChannelController::hasPermission(const HttpRequestPtr& req, const vector<string>& allowedRoles) {
    auto authHeader = req->getHeader("Authorization");
    if (authHeader.empty() || authHeader.find("Bearer ") != 0) {
        return false;
    }

    string token = authHeader.substr(7);
    string userRole = getRoleFromToken(token);

    return find(allowedRoles.begin(), allowedRoles.end(), userRole) != allowedRoles.end();
}

// Вспомогательный метод для создания JSON ответа пользователя
Json::Value ChannelController::getUserJsonResponse(const Users& user) {
    Json::Value userJson;

    try {
        userJson["id"] = user.getValueOfId();
        userJson["username"] = user.getValueOfUsername();
        userJson["role"] = user.getValueOfRole();
        userJson["profile_is_public"] = user.getValueOfProfileIsPublic();

        // Обработка nullable полей
        try {
            string avatarPath = user.getValueOfAvatarPath();
            if (!avatarPath.empty()) {
                userJson["avatar_path"] = avatarPath;
            } else {
                userJson["avatar_path"] = Json::nullValue;
            }
        } catch (const exception& e) {
            userJson["avatar_path"] = Json::nullValue;
        }

        try {
            string coverPath = user.getValueOfCoverPath();
            if (!coverPath.empty()) {
                userJson["cover_path"] = coverPath;
            } else {
                userJson["cover_path"] = Json::nullValue;
            }
        } catch (const exception& e) {
            userJson["cover_path"] = Json::nullValue;
        }

        // Парсим JSON поля
        try {
            string contactsStr = user.getValueOfContacts();
            if (!contactsStr.empty()) {
                Json::Value contacts;
                Json::Reader reader;
                if (reader.parse(contactsStr, contacts)) {
                    // Фильтруем только публичные контакты для канала
                    Json::Value publicContacts(Json::arrayValue);
                    for (const auto& contact : contacts) {
                        if (contact.isMember("is_public") && contact["is_public"].asBool()) {
                            Json::Value publicContact;
                            publicContact["name"] = contact["name"];
                            publicContact["url"] = contact["url"];
                            if (contact.isMember("icon")) {
                                publicContact["icon"] = contact["icon"];
                            }
                            publicContacts.append(publicContact);
                        }
                    }
                    userJson["contacts"] = publicContacts;
                } else {
                    userJson["contacts"] = Json::arrayValue;
                }
            } else {
                userJson["contacts"] = Json::arrayValue;
            }
        } catch (const exception& e) {
            userJson["contacts"] = Json::arrayValue;
        }

        try {
            string informationStr = user.getValueOfInformation();
            if (!informationStr.empty()) {
                Json::Value information;
                Json::Reader reader;
                if (reader.parse(informationStr, information)) {
                    // Фильтруем только публичную информацию для канала
                    Json::Value publicInformation(Json::arrayValue);
                    for (const auto& info : information) {
                        if (info.isMember("is_public") && info["is_public"].asBool()) {
                            Json::Value publicInfo;
                            publicInfo["label"] = info["label"];
                            publicInfo["value"] = info["value"];
                            publicInformation.append(publicInfo);
                        }
                    }
                    userJson["information"] = publicInformation;
                } else {
                    userJson["information"] = Json::arrayValue;
                }
            } else {
                userJson["information"] = Json::arrayValue;
            }
        } catch (const exception& e) {
            userJson["information"] = Json::arrayValue;
        }

        userJson["created_at"] = user.getValueOfCreatedAt().toDbStringLocal();

        try {
            auto lastLogin = user.getValueOfLastLoginAt();
            if (lastLogin != trantor::Date()) {
                userJson["last_login_at"] = lastLogin.toDbStringLocal();
            } else {
                userJson["last_login_at"] = Json::nullValue;
            }
        } catch (const exception& e) {
            userJson["last_login_at"] = Json::nullValue;
        }

    } catch (const exception& e) {
        LOG_ERROR << "Error creating user JSON response: " << e.what();
    }

    return userJson;
}

// Вспомогательный метод для создания JSON ответа курса
Json::Value ChannelController::getCourseJsonResponse(const Courses& course) {
    Json::Value courseJson;

    try {
        courseJson["id"] = course.getValueOfId();
        courseJson["title"] = course.getValueOfTitle();

        // Исправлено: проверяем описание на пустоту
        string description = course.getValueOfDescription();
        courseJson["description"] = description.empty() ? "" : description;

        courseJson["category"] = course.getValueOfCategory();
        courseJson["level"] = course.getValueOfLevel();
        courseJson["language"] = course.getValueOfLanguage();

        courseJson["chapters_count"] = course.getValueOfChaptersCount();
        courseJson["videos_count"] = course.getValueOfVideosCount();
        courseJson["total_views"] = course.getValueOfTotalViews();
        courseJson["total_likes"] = course.getValueOfTotalLikes();

        // Исправлено: преобразуем цену и рейтинг
        try {
            string priceStr = course.getValueOfPrice();
            courseJson["price"] = priceStr.empty() ? 0.0 : stod(priceStr);
        } catch (const exception& e) {
            courseJson["price"] = 0.0;
        }

        courseJson["is_paid"] = course.getValueOfIsPaid();
        courseJson["is_published"] = course.getValueOfIsPublished();
        courseJson["is_public"] = course.getValueOfIsPublic();

        // Исправлено: преобразуем рейтинг
        try {
            string ratingStr = course.getValueOfRating();
            courseJson["rating"] = ratingStr.empty() ? 0.0 : stod(ratingStr);
        } catch (const exception& e) {
            courseJson["rating"] = 0.0;
        }

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

// Получить информацию о канале
void ChannelController::getChannelInfo(const HttpRequestPtr& req,
                                       function<void(const HttpResponsePtr&)>&& callback,
                                       const string& channelId) {

    // Валидация UUID
    if (!isValidUUID(channelId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid channel ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();
    string currentUserId = getCurrentUserId(req);
    bool isOwner = (currentUserId == channelId);
    bool hasAdminAccess = hasPermission(req, {"основатель", "админ"});

    dbClient->execSqlAsync(
        "SELECT u.*, us.subscribers_count, us.completed_courses, us.study_hours, "
        "us.created_courses, us.total_likes, us.total_views "
        "FROM users u "
        "LEFT JOIN user_stats us ON u.id = us.user_id "
        "WHERE u.id = $1",
        [callback, isOwner, hasAdminAccess, this](const Result& result) {
            if (result.empty()) {
                auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Channel not found"));
                resp->setStatusCode(k404NotFound);
                callback(resp);
                return;
            }

            try {
                auto user = Users(result[0]);

                // Проверяем доступ к каналу
                bool isPublic = user.getValueOfProfileIsPublic();
                if (!isPublic && !isOwner && !hasAdminAccess) {
                    auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Channel is private"));
                    resp->setStatusCode(k403Forbidden);
                    callback(resp);
                    return;
                }

                // Создаем ответ с информацией о канале
                Json::Value channelInfo = getUserJsonResponse(user);

                // Добавляем статистику
                if (!result[0]["subscribers_count"].isNull()) {
                    channelInfo["stats"] = Json::Value();
                    channelInfo["stats"]["subscribers_count"] = result[0]["subscribers_count"].as<int>();
                    channelInfo["stats"]["completed_courses"] = result[0]["completed_courses"].as<int>();
                    channelInfo["stats"]["study_hours"] = result[0]["study_hours"].as<int>();
                    channelInfo["stats"]["created_courses"] = result[0]["created_courses"].as<int>();
                    channelInfo["stats"]["total_likes"] = result[0]["total_likes"].as<int>();
                    channelInfo["stats"]["total_views"] = result[0]["total_views"].as<int>();
                }

                auto resp = HttpResponse::newHttpJsonResponse(channelInfo);
                callback(resp);

            } catch (const exception& e) {
                LOG_ERROR << "Error processing channel data: " << e.what();
                auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Error processing channel data"));
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
        channelId
        );
}

// Получить курсы канала
void ChannelController::getChannelCourses(const HttpRequestPtr& req,
                                          function<void(const HttpResponsePtr&)>&& callback,
                                          const string& channelId) {

    // Валидация UUID
    if (!isValidUUID(channelId)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid channel ID format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Получаем параметры запроса с исправленным методом
    auto paramsMap = req->getParameters();
    string pageStr = paramsMap.find("page") != paramsMap.end() ? paramsMap["page"] : "1";
    string limitStr = paramsMap.find("limit") != paramsMap.end() ? paramsMap["limit"] : "10";
    string category = paramsMap.find("category") != paramsMap.end() ? paramsMap["category"] : "";
    string level = paramsMap.find("level") != paramsMap.end() ? paramsMap["level"] : "";
    string sortBy = paramsMap.find("sort_by") != paramsMap.end() ? paramsMap["sort_by"] : "created_at";
    string sortOrder = paramsMap.find("sort_order") != paramsMap.end() ? paramsMap["sort_order"] : "desc";

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

    // Проверяем существование канала и его доступность
    auto dbClient = app().getDbClient();
    string currentUserId = getCurrentUserId(req);
    bool isOwner = (currentUserId == channelId);
    bool hasAdminAccess = hasPermission(req, {"основатель", "админ"});

    dbClient->execSqlAsync(
        "SELECT profile_is_public FROM users WHERE id = $1",
        [callback, channelId, isOwner, hasAdminAccess, category, level, sortBy, sortOrder, page, limit, offset, dbClient, this](
            const Result& userResult) {

            if (userResult.empty()) {
                auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Channel not found"));
                resp->setStatusCode(k404NotFound);
                callback(resp);
                return;
            }

            // Проверяем доступ к каналу
            bool isPublic = userResult[0]["profile_is_public"].as<bool>();
            if (!isPublic && !isOwner && !hasAdminAccess) {
                auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Channel is private"));
                resp->setStatusCode(k403Forbidden);
                callback(resp);
                return;
            }

            // Строим SQL запрос для курсов
            string sql = "SELECT * FROM courses WHERE author_id = $1 ";
            vector<string> params = {channelId};

            // Определяем условия видимости курсов
            if (!isOwner && !hasAdminAccess) {
                sql += "AND is_published = true AND is_public = true ";
            }

            // Добавляем фильтры
            int paramCount = 1;
            if (!category.empty()) {
                paramCount++;
                sql += "AND category = $" + to_string(paramCount) + " ";
                params.push_back(category);
            }

            if (!level.empty()) {
                paramCount++;
                sql += "AND level = $" + to_string(paramCount) + " ";
                params.push_back(level);
            }

            // Добавляем сортировку
            vector<string> allowedSortFields = {"created_at", "updated_at", "title", "rating", "total_views"};
            string safeSortBy = find(allowedSortFields.begin(), allowedSortFields.end(), sortBy) != allowedSortFields.end()
                                    ? sortBy : "created_at";
            string safeSortOrder = (sortOrder == "asc" || sortOrder == "desc") ? sortOrder : "desc";

            sql += "ORDER BY " + safeSortBy + " " + safeSortOrder + " ";

            // Исправлено: разделяем инкременты paramCount
            paramCount++;
            sql += "LIMIT $" + to_string(paramCount) + " ";
            params.push_back(to_string(limit));

            paramCount++;
            sql += "OFFSET $" + to_string(paramCount);
            params.push_back(to_string(offset));

            // Функция для обработки результата курсов
            auto processCoursesResult = [this, callback, channelId, page, limit, dbClient](const Result& coursesResult) {
                // Получаем общее количество курсов для пагинации
                string countSql = "SELECT COUNT(*) as total FROM courses WHERE author_id = $1 AND is_published = true AND is_public = true";
                dbClient->execSqlAsync(
                    countSql,
                    [callback, coursesResult, channelId, page, limit, this](const Result& countResult) {

                        Json::Value response;
                        Json::Value coursesArray(Json::arrayValue);

                        // Формируем массив курсов
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

                        if (!countResult.empty()) {
                            int totalCourses = countResult[0]["total"].as<int>();
                            response["pagination"]["total"] = totalCourses;
                            response["pagination"]["pages"] = (totalCourses + limit - 1) / limit;
                        } else {
                            response["pagination"]["total"] = 0;
                            response["pagination"]["pages"] = 0;
                        }

                        response["channel_id"] = channelId;

                        auto resp = HttpResponse::newHttpJsonResponse(response);
                        callback(resp);
                    },
                    [callback, this](const DrogonDbException& e) {
                        LOG_ERROR << "Database error counting courses: " << e.base().what();
                        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                        resp->setStatusCode(k500InternalServerError);
                        callback(resp);
                    },
                    channelId
                    );
            };

            // Выполняем запрос в зависимости от количества параметров
            switch (params.size()) {
            case 1:
                dbClient->execSqlAsync(
                    sql,
                    [processCoursesResult](const Result& result) {
                        processCoursesResult(result);
                    },
                    [callback, this](const DrogonDbException& e) {
                        LOG_ERROR << "Database error fetching courses: " << e.base().what();
                        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                        resp->setStatusCode(k500InternalServerError);
                        callback(resp);
                    },
                    params[0]
                    );
                break;
            case 2:
                dbClient->execSqlAsync(
                    sql,
                    [processCoursesResult](const Result& result) {
                        processCoursesResult(result);
                    },
                    [callback, this](const DrogonDbException& e) {
                        LOG_ERROR << "Database error fetching courses: " << e.base().what();
                        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                        resp->setStatusCode(k500InternalServerError);
                        callback(resp);
                    },
                    params[0], params[1]
                    );
                break;
            case 3:
                dbClient->execSqlAsync(
                    sql,
                    [processCoursesResult](const Result& result) {
                        processCoursesResult(result);
                    },
                    [callback, this](const DrogonDbException& e) {
                        LOG_ERROR << "Database error fetching courses: " << e.base().what();
                        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                        resp->setStatusCode(k500InternalServerError);
                        callback(resp);
                    },
                    params[0], params[1], params[2]
                    );
                break;
            case 4:
                dbClient->execSqlAsync(
                    sql,
                    [processCoursesResult](const Result& result) {
                        processCoursesResult(result);
                    },
                    [callback, this](const DrogonDbException& e) {
                        LOG_ERROR << "Database error fetching courses: " << e.base().what();
                        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                        resp->setStatusCode(k500InternalServerError);
                        callback(resp);
                    },
                    params[0], params[1], params[2], params[3]
                    );
                break;
            case 5:
                dbClient->execSqlAsync(
                    sql,
                    [processCoursesResult](const Result& result) {
                        processCoursesResult(result);
                    },
                    [callback, this](const DrogonDbException& e) {
                        LOG_ERROR << "Database error fetching courses: " << e.base().what();
                        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                        resp->setStatusCode(k500InternalServerError);
                        callback(resp);
                    },
                    params[0], params[1], params[2], params[3], params[4]
                    );
                break;
            default:
                auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid query parameters"));
                resp->setStatusCode(k400BadRequest);
                callback(resp);
            }

        },
        [callback, this](const DrogonDbException& e) {
            LOG_ERROR << "Database error checking channel: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        channelId
        );
}
