#include "UserController.h"
#include <drogon/HttpResponse.h>
#include <drogon/utils/Utilities.h>
#include <drogon/drogon.h>
#include <algorithm>
#include <filesystem>
#include <bcrypt/BCrypt.hpp>

using namespace drogon;
using namespace drogon::orm;
using namespace std;

// Вспомогательная функция для создания JSON ответов
Json::Value UserController::createJsonResponse(const string& key, const string& value) {
    Json::Value json;
    json[key] = value;
    return json;
}

// JWT аутентификация
string UserController::getCurrentUserId(const HttpRequestPtr& req) {
    auto authHeader = req->getHeader("Authorization");
    if (authHeader.empty() || authHeader.find("Bearer ") != 0) {
        return "";
    }

    string token = authHeader.substr(7);
    return getUserIdFromToken(token);
}

bool UserController::validateJWT(const string& token) {
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

string UserController::getUserIdFromToken(const string& token) {
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

string UserController::getRoleFromToken(const string& token) {
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

bool UserController::hasPermission(const HttpRequestPtr& req, const vector<string>& allowedRoles) {
    auto authHeader = req->getHeader("Authorization");
    if (authHeader.empty() || authHeader.find("Bearer ") != 0) {
        return false;
    }

    string token = authHeader.substr(7);
    string userRole = getRoleFromToken(token);

    return find(allowedRoles.begin(), allowedRoles.end(), userRole) != allowedRoles.end();
}

// Валидация
bool UserController::isValidUserRole(const string& role) {
    vector<string> validRoles = {"основатель", "админ", "проверяющий", "пользователь"};
    return find(validRoles.begin(), validRoles.end(), role) != validRoles.end();
}

bool UserController::isValidInformationItem(const Json::Value& item) {
    return item.isMember("label") && item.isMember("value") && item.isMember("is_public");
}

bool UserController::isValidContactItem(const Json::Value& item) {
    return item.isMember("name") && item.isMember("url") && item.isMember("is_public");
}

bool UserController::isValidProgressData(const Json::Value& data) {
    return data.isMember("video_id") && data.isMember("completed") && data.isMember("watched_seconds");
}

// Работа с файлами
string UserController::saveUploadedFile(const HttpRequestPtr& req, const string& uploadPath) {
    MultiPartParser parser;
    if (parser.parse(req) != 0) {
        return "";
    }

    auto files = parser.getFiles();
    if (files.empty()) {
        return "";
    }

    auto& file = files[0];
    string fileName = utils::getUuid() + filesystem::path(file.getFileName()).extension().string();
    string fullPath = uploadPath + fileName;

    filesystem::create_directories(uploadPath);
    file.saveAs(fullPath);

    return fullPath;
}

bool UserController::deleteFile(const string& filePath) {
    try {
        if (filePath.empty()) {
            LOG_DEBUG << "File path is empty";
            return true;
        }

        filesystem::path path(filePath);

        LOG_DEBUG << "Attempting to delete file: " << filePath;
        LOG_DEBUG << "Absolute path: " << filesystem::absolute(path).string();
        LOG_DEBUG << "Current working directory: " << filesystem::current_path().string();

        if (!filesystem::exists(path)) {
            LOG_DEBUG << "File does not exist: " << filePath;
            return true; // Файл уже удален
        }

        if (filesystem::is_directory(path)) {
            LOG_ERROR << "Path is a directory, not a file: " << filePath;
            return false;
        }

        // Получаем информацию о файле перед удалением
        auto fileSize = filesystem::file_size(path);
        auto permissions = filesystem::status(path).permissions();

        LOG_DEBUG << "File size: " << fileSize << " bytes";
        LOG_DEBUG << "File permissions: " << static_cast<int>(permissions);

        // Пытаемся удалить файл
        bool removed = filesystem::remove(path);

        if (removed) {
            LOG_DEBUG << "Successfully deleted file: " << filePath << " (size: " << fileSize << " bytes)";

            // Проверяем, что файл действительно удален
            if (filesystem::exists(path)) {
                LOG_ERROR << "File still exists after deletion: " << filePath;
                return false;
            }
            return true;
        } else {
            LOG_ERROR << "Failed to delete file (filesystem::remove returned false): " << filePath;

            // Пробуем альтернативные методы удаления
            try {
                // Пробуем изменить права и удалить снова
                filesystem::permissions(path, filesystem::perms::all);
                removed = filesystem::remove(path);

                if (removed) {
                    LOG_DEBUG << "Successfully deleted file after permission change: " << filePath;
                    return true;
                }
            } catch (const exception& e) {
                LOG_ERROR << "Alternative deletion failed: " << e.what();
            }

            return false;
        }
    } catch (const filesystem::filesystem_error& e) {
        LOG_ERROR << "File system error deleting " << filePath << ": " << e.what();
        return false;
    } catch (const exception& e) {
        LOG_ERROR << "Unexpected error deleting " << filePath << ": " << e.what();
        return false;
    }
}

// JSON обработка
Json::Value UserController::updateJsonArray(const Json::Value& originalArray,
                                            const Json::Value& newItem,
                                            const string& itemId) {
    Json::Value updatedArray = originalArray;
    bool found = false;

    for (Json::Value::ArrayIndex i = 0; i < updatedArray.size(); ++i) {
        if (updatedArray[i]["id"].asString() == itemId) {
            updatedArray[i] = newItem;
            updatedArray[i]["id"] = itemId;
            found = true;
            break;
        }
    }

    if (!found) {
        string newId = itemId.empty() ? utils::getUuid() : itemId;
        Json::Value itemWithId = newItem;
        itemWithId["id"] = newId;
        updatedArray.append(itemWithId);
    }

    return updatedArray;
}

Json::Value UserController::removeFromJsonArray(const Json::Value& originalArray,
                                                const string& itemId) {
    Json::Value updatedArray(Json::arrayValue);

    for (const auto& item : originalArray) {
        if (item["id"].asString() != itemId) {
            updatedArray.append(item);
        }
    }

    return updatedArray;
}

// Вспомогательный метод для создания JSON ответа пользователя
Json::Value UserController::getUserJsonResponse(const Users& user) {
    Json::Value userJson;

    try {
        userJson["id"] = user.getValueOfId();
        userJson["username"] = user.getValueOfUsername();
        userJson["email"] = user.getValueOfEmail();
        userJson["role"] = user.getValueOfRole();
        userJson["profile_is_public"] = user.getValueOfProfileIsPublic();

        // Обработка nullable полей
        try {
            if (!user.getValueOfAvatarPath().empty()) {
                userJson["avatar_path"] = user.getValueOfAvatarPath();
            } else {
                userJson["avatar_path"] = Json::nullValue;
            }
        } catch (const exception& e) {
            userJson["avatar_path"] = Json::nullValue;
        }

        try {
            if (!user.getValueOfCoverPath().empty()) {
                userJson["cover_path"] = user.getValueOfCoverPath();
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
                    userJson["contacts"] = contacts;
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
                    userJson["information"] = information;
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
        userJson["updated_at"] = user.getValueOfUpdatedAt().toDbStringLocal();

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

// Основные операции с профилем
void UserController::getMyProfile(const HttpRequestPtr& req,
                                  function<void(const HttpResponsePtr&)>&& callback) {
    auto userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    dbClient->execSqlAsync(
        "SELECT * FROM users WHERE id = $1",
        [callback, this](const Result& result) {
            if (result.empty()) {
                auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "User not found"));
                resp->setStatusCode(k404NotFound);
                callback(resp);
                return;
            }

            try {
                auto user = Users(result[0]);
                auto userJson = getUserJsonResponse(user);
                auto resp = HttpResponse::newHttpJsonResponse(userJson);
                callback(resp);
            } catch (const exception& e) {
                LOG_ERROR << "Error processing user data: " << e.what();
                auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Error processing user data"));
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
        userId
        );
}

void UserController::updateMyProfile(const HttpRequestPtr& req,
                                     function<void(const HttpResponsePtr&)>&& callback) {
    auto userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    auto json = req->getJsonObject();
    if (!json) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid JSON"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Проверяем уникальность username
    if (json->isMember("username")) {
        string newUsername = (*json)["username"].asString();
        auto dbClient = app().getDbClient();

        try {
            auto result = dbClient->execSqlSync(
                "SELECT id FROM users WHERE username = $1 AND id != $2",
                newUsername, userId
                );

            if (!result.empty()) {
                auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Username already taken"));
                resp->setStatusCode(k400BadRequest);
                callback(resp);
                return;
            }
        } catch (const exception& e) {
            LOG_ERROR << "Username check error: " << e.what();
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
            return;
        }
    }

    // Собираем поля для обновления
    vector<string> updateFields;
    vector<string> params;

    if (json->isMember("username")) {
        updateFields.push_back("username = $" + to_string(params.size() + 1));
        params.push_back((*json)["username"].asString());
    }

    if (json->isMember("profile_is_public")) {
        updateFields.push_back("profile_is_public = $" + to_string(params.size() + 1));
        params.push_back((*json)["profile_is_public"].asBool() ? "true" : "false");
    }

    if (updateFields.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "No fields to update"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    updateFields.push_back("updated_at = NOW()");
    params.push_back(userId);

    // Строим SQL запрос
    string sql = "UPDATE users SET ";
    for (size_t i = 0; i < updateFields.size(); ++i) {
        if (i > 0) sql += ", ";
        sql += updateFields[i];
    }
    sql += " WHERE id = $" + to_string(params.size());

    auto dbClient = app().getDbClient();

    // Выполняем запрос в зависимости от количества параметров
    switch (params.size()) {
    case 2:
        dbClient->execSqlAsync(
            sql,
            [callback](const Result& result) {
                Json::Value response;
                response["message"] = "Profile updated successfully";
                response["success"] = true;
                auto resp = HttpResponse::newHttpJsonResponse(response);
                callback(resp);
            },
            [callback, this](const DrogonDbException& e) {
                LOG_ERROR << "Database error: " << e.base().what();
                Json::Value response;
                response["error"] = "Failed to update profile";
                response["success"] = false;
                auto resp = HttpResponse::newHttpJsonResponse(response);
                resp->setStatusCode(k500InternalServerError);
                callback(resp);
            },
            params[0], params[1]
            );
        break;
    case 3:
        dbClient->execSqlAsync(
            sql,
            [callback](const Result& result) {
                Json::Value response;
                response["message"] = "Profile updated successfully";
                response["success"] = true;
                auto resp = HttpResponse::newHttpJsonResponse(response);
                callback(resp);
            },
            [callback, this](const DrogonDbException& e) {
                LOG_ERROR << "Database error: " << e.base().what();
                Json::Value response;
                response["error"] = "Failed to update profile";
                response["success"] = false;
                auto resp = HttpResponse::newHttpJsonResponse(response);
                resp->setStatusCode(k500InternalServerError);
                callback(resp);
            },
            params[0], params[1], params[2]
            );
        break;
    default:
        Json::Value response;
        response["error"] = "Invalid update parameters";
        response["success"] = false;
        auto resp = HttpResponse::newHttpJsonResponse(response);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
    }
}

void UserController::changeUserRole(const HttpRequestPtr& req,
                                    function<void(const HttpResponsePtr&)>&& callback,
                                    const string& userId) {
    // Проверяем права доступа
    if (!hasPermission(req, {"основатель", "админ"})) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Forbidden"));
        resp->setStatusCode(k403Forbidden);
        callback(resp);
        return;
    }

    auto json = req->getJsonObject();
    if (!json || !json->isMember("role") || !isValidUserRole((*json)["role"].asString())) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid role"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    string newRole = (*json)["role"].asString();
    auto dbClient = app().getDbClient();

    dbClient->execSqlAsync(
        "UPDATE users SET role = $1, updated_at = NOW() WHERE id = $2",
        [callback, userId, newRole, this](const Result& result) {
            if (result.affectedRows() == 0) {
                auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "User not found"));
                resp->setStatusCode(k404NotFound);
                callback(resp);
                return;
            }

            Json::Value response;
            response["message"] = "User role updated successfully";
            response["user_id"] = userId;
            response["new_role"] = newRole;
            response["success"] = true;
            auto resp = HttpResponse::newHttpJsonResponse(response);
            callback(resp);
        },
        [callback, this](const DrogonDbException& e) {
            LOG_ERROR << "Database error: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        newRole, userId
        );
}

// Аватар
void UserController::uploadAvatar(const HttpRequestPtr& req,
                                  function<void(const HttpResponsePtr&)>&& callback) {
    auto userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    string fullPath = saveUploadedFile(req, "uploads/avatars/");
    if (fullPath.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "No file uploaded"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    dbClient->execSqlAsync(
        "UPDATE users SET avatar_path = $1, updated_at = NOW() WHERE id = $2",
        [callback, fullPath](const Result& result) {
            Json::Value response;
            response["message"] = "Avatar uploaded successfully";
            response["avatar_path"] = fullPath;
            response["success"] = true;
            auto resp = HttpResponse::newHttpJsonResponse(response);
            callback(resp);
        },
        [callback, fullPath, this](const DrogonDbException& e) {
            // Удаляем файл в случае ошибки
            filesystem::remove(fullPath);
            LOG_ERROR << "Database error: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Failed to update avatar"));
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        fullPath, userId
        );
}

void UserController::updateAvatar(const HttpRequestPtr& req,
                                  function<void(const HttpResponsePtr&)>&& callback) {
    auto userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    // Сначала получаем текущий аватар
    auto dbClient = app().getDbClient();

    dbClient->execSqlAsync(
        "SELECT avatar_path FROM users WHERE id = $1",
        [req, callback, userId, this](const Result& result) {
            if (result.empty()) {
                auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "User not found"));
                resp->setStatusCode(k404NotFound);
                callback(resp);
                return;
            }

            // Удаляем старый аватар
            if (!result[0]["avatar_path"].isNull()) {
                string oldAvatarPath = result[0]["avatar_path"].as<string>();
                if (!oldAvatarPath.empty()) {
                    deleteFile(oldAvatarPath);
                }
            }

            // Загружаем новый с новым callback
            auto uploadCallback = [callback](const HttpResponsePtr& resp) {
                callback(resp);
            };
            uploadAvatar(req, std::move(uploadCallback));
        },
        [callback, this](const DrogonDbException& e) {
            LOG_ERROR << "Database error: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        userId
        );
}

void UserController::deleteAvatar(const HttpRequestPtr& req,
                                  function<void(const HttpResponsePtr&)>&& callback) {
    auto userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    try {
        // Синхронно получаем путь к аватару
        auto result = dbClient->execSqlSync("SELECT avatar_path FROM users WHERE id = $1", userId);

        if (result.empty()) {
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "User not found"));
            resp->setStatusCode(k404NotFound);
            callback(resp);
            return;
        }

        string avatarPath;
        bool hasAvatar = false;

        // Получаем путь к аватару
        if (!result[0]["avatar_path"].isNull()) {
            avatarPath = result[0]["avatar_path"].as<string>();
            if (!avatarPath.empty()) {
                hasAvatar = true;
                LOG_DEBUG << "Found avatar path: " << avatarPath;
            }
        }

        // Удаляем файл аватара если он существует
        bool fileDeleted = true;
        if (hasAvatar) {
            fileDeleted = deleteFile(avatarPath);
            if (!fileDeleted) {
                LOG_ERROR << "Failed to delete avatar file: " << avatarPath;
            } else {
                LOG_DEBUG << "Successfully deleted avatar file: " << avatarPath;
            }
        }

        // Синхронно обновляем базу данных
        auto updateResult = dbClient->execSqlSync(
            "UPDATE users SET avatar_path = NULL, updated_at = NOW() WHERE id = $1",
            userId
            );

        Json::Value response;
        if (updateResult.affectedRows() > 0) {
            response["message"] = "Avatar deleted successfully";
            response["success"] = true;
            if (hasAvatar && !fileDeleted) {
                response["warning"] = "Avatar removed from database but file deletion failed";
            }
        } else {
            response["error"] = "Failed to update database";
            response["success"] = false;
        }

        auto resp = HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const DrogonDbException& e) {
        LOG_ERROR << "Database error: " << e.base().what();
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
    } catch (const exception& e) {
        LOG_ERROR << "Unexpected error: " << e.what();
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Internal server error"));
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
    }
}

void UserController::deleteCover(const HttpRequestPtr& req,
                                 function<void(const HttpResponsePtr&)>&& callback) {
    auto userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    try {
        // Синхронно получаем путь к обложке
        auto result = dbClient->execSqlSync("SELECT cover_path FROM users WHERE id = $1", userId);

        if (result.empty()) {
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "User not found"));
            resp->setStatusCode(k404NotFound);
            callback(resp);
            return;
        }

        string coverPath;
        bool hasCover = false;

        // Получаем путь к обложке
        if (!result[0]["cover_path"].isNull()) {
            coverPath = result[0]["cover_path"].as<string>();
            if (!coverPath.empty()) {
                hasCover = true;
                LOG_DEBUG << "Found cover path: " << coverPath;
            }
        }

        // Удаляем файл обложки если он существует
        bool fileDeleted = true;
        if (hasCover) {
            fileDeleted = deleteFile(coverPath);
            if (!fileDeleted) {
                LOG_ERROR << "Failed to delete cover file: " << coverPath;
            } else {
                LOG_DEBUG << "Successfully deleted cover file: " << coverPath;
            }
        }

        // Синхронно обновляем базу данных
        auto updateResult = dbClient->execSqlSync(
            "UPDATE users SET cover_path = NULL, updated_at = NOW() WHERE id = $1",
            userId
            );

        Json::Value response;
        if (updateResult.affectedRows() > 0) {
            response["message"] = "Cover deleted successfully";
            response["success"] = true;
            if (hasCover && !fileDeleted) {
                response["warning"] = "Cover removed from database but file deletion failed";
            }
        } else {
            response["error"] = "Failed to update database";
            response["success"] = false;
        }

        auto resp = HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const DrogonDbException& e) {
        LOG_ERROR << "Database error: " << e.base().what();
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
    } catch (const exception& e) {
        LOG_ERROR << "Unexpected error: " << e.what();
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Internal server error"));
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
    }
}

// Обложка (аналогично аватару)
void UserController::uploadCover(const HttpRequestPtr& req,
                                 function<void(const HttpResponsePtr&)>&& callback) {
    auto userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    string fullPath = saveUploadedFile(req, "uploads/covers/");
    if (fullPath.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "No file uploaded"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    dbClient->execSqlAsync(
        "UPDATE users SET cover_path = $1, updated_at = NOW() WHERE id = $2",
        [callback, fullPath](const Result& result) {
            Json::Value response;
            response["message"] = "Cover uploaded successfully";
            response["cover_path"] = fullPath;
            response["success"] = true;
            auto resp = HttpResponse::newHttpJsonResponse(response);
            callback(resp);
        },
        [callback, fullPath, this](const DrogonDbException& e) {
            filesystem::remove(fullPath);
            LOG_ERROR << "Database error: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Failed to update cover"));
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        fullPath, userId
        );
}

void UserController::updateCover(const HttpRequestPtr& req,
                                 function<void(const HttpResponsePtr&)>&& callback) {
    auto userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    dbClient->execSqlAsync(
        "SELECT cover_path FROM users WHERE id = $1",
        [req, callback, userId, this](const Result& result) {
            if (result.empty()) {
                auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "User not found"));
                resp->setStatusCode(k404NotFound);
                callback(resp);
                return;
            }

            if (!result[0]["cover_path"].isNull()) {
                string oldCoverPath = result[0]["cover_path"].as<string>();
                if (!oldCoverPath.empty()) {
                    deleteFile(oldCoverPath);
                }
            }

            auto uploadCallback = [callback](const HttpResponsePtr& resp) {
                callback(resp);
            };
            uploadCover(req, std::move(uploadCallback));
        },
        [callback, this](const DrogonDbException& e) {
            LOG_ERROR << "Database error: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        userId
        );
}

// Информация
void UserController::getInformation(const HttpRequestPtr& req,
                                    function<void(const HttpResponsePtr&)>&& callback) {
    auto userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    dbClient->execSqlAsync(
        "SELECT information FROM users WHERE id = $1",
        [callback, this](const Result& result) {
            if (result.empty()) {
                auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "User not found"));
                resp->setStatusCode(k404NotFound);
                callback(resp);
                return;
            }

            Json::Value information;
            if (!result[0]["information"].isNull()) {
                string informationStr = result[0]["information"].as<string>();
                if (!informationStr.empty()) {
                    Json::Reader reader;
                    if (!reader.parse(informationStr, information)) {
                        information = Json::arrayValue;
                    }
                } else {
                    information = Json::arrayValue;
                }
            } else {
                information = Json::arrayValue;
            }

            auto resp = HttpResponse::newHttpJsonResponse(information);
            callback(resp);
        },
        [callback, this](const DrogonDbException& e) {
            LOG_ERROR << "Database error: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        userId
        );
}

void UserController::addInformation(const HttpRequestPtr& req,
                                    function<void(const HttpResponsePtr&)>&& callback) {
    auto userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    auto json = req->getJsonObject();
    if (!json || !isValidInformationItem(*json)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid information item"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    dbClient->execSqlAsync(
        "SELECT information FROM users WHERE id = $1",
        [callback, userId, json, this](const Result& result) {
            if (result.empty()) {
                auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "User not found"));
                resp->setStatusCode(k404NotFound);
                callback(resp);
                return;
            }

            Json::Value informationArray;
            if (!result[0]["information"].isNull()) {
                string informationStr = result[0]["information"].as<string>();
                if (!informationStr.empty()) {
                    Json::Reader reader;
                    reader.parse(informationStr, informationArray);
                }
            }

            // Добавляем новый элемент
            string newId = utils::getUuid();
            (*json)["id"] = newId;
            informationArray.append(*json);

            // Обновляем базу данных
            Json::FastWriter writer;
            string informationStr = writer.write(informationArray);

            auto dbClient = app().getDbClient();
            dbClient->execSqlAsync(
                "UPDATE users SET information = $1, updated_at = NOW() WHERE id = $2",
                [callback, newId](const Result& result) {
                    Json::Value response;
                    response["message"] = "Information added successfully";
                    response["id"] = newId;
                    response["success"] = true;
                    auto resp = HttpResponse::newHttpJsonResponse(response);
                    callback(resp);
                },
                [callback, this](const DrogonDbException& e) {
                    LOG_ERROR << "Database error: " << e.base().what();
                    auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                    resp->setStatusCode(k500InternalServerError);
                    callback(resp);
                },
                informationStr, userId
                );
        },
        [callback, this](const DrogonDbException& e) {
            LOG_ERROR << "Database error: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        userId
        );
}

void UserController::updateAllInformation(const HttpRequestPtr& req,
                                          function<void(const HttpResponsePtr&)>&& callback) {
    auto userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    auto json = req->getJsonObject();
    if (!json || !json->isArray()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid data format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Валидируем все элементы
    for (const auto& item : *json) {
        if (!isValidInformationItem(item)) {
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid information item"));
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            return;
        }
    }

    Json::FastWriter writer;
    string informationStr = writer.write(*json);

    auto dbClient = app().getDbClient();

    dbClient->execSqlAsync(
        "UPDATE users SET information = $1, updated_at = NOW() WHERE id = $2",
        [callback](const Result& result) {
            Json::Value response;
            response["message"] = "All information updated successfully";
            response["success"] = true;
            auto resp = HttpResponse::newHttpJsonResponse(response);
            callback(resp);
        },
        [callback, this](const DrogonDbException& e) {
            LOG_ERROR << "Database error: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        informationStr, userId
        );
}

void UserController::updateInformationItem(const HttpRequestPtr& req,
                                           function<void(const HttpResponsePtr&)>&& callback,
                                           const string& infoId) {
    auto userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    auto json = req->getJsonObject();
    if (!json || !isValidInformationItem(*json)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid information item"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    dbClient->execSqlAsync(
        "SELECT information FROM users WHERE id = $1",
        [callback, userId, infoId, json, this](const Result& result) {
            if (result.empty()) {
                auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "User not found"));
                resp->setStatusCode(k404NotFound);
                callback(resp);
                return;
            }

            Json::Value informationArray;
            if (!result[0]["information"].isNull()) {
                string informationStr = result[0]["information"].as<string>();
                if (!informationStr.empty()) {
                    Json::Reader reader;
                    reader.parse(informationStr, informationArray);
                }
            }

            // Обновляем конкретный элемент
            Json::Value updatedArray = Json::arrayValue;
            bool found = false;

            for (const auto& item : informationArray) {
                if (item["id"].asString() == infoId) {
                    Json::Value updatedItem = *json;
                    updatedItem["id"] = infoId;
                    updatedArray.append(updatedItem);
                    found = true;
                } else {
                    updatedArray.append(item);
                }
            }

            if (!found) {
                Json::Value newItem = *json;
                newItem["id"] = infoId;
                updatedArray.append(newItem);
            }

            Json::FastWriter writer;
            string informationStr = writer.write(updatedArray);

            auto dbClient = app().getDbClient();
            dbClient->execSqlAsync(
                "UPDATE users SET information = $1, updated_at = NOW() WHERE id = $2",
                [callback](const Result& result) {
                    Json::Value response;
                    response["message"] = "Information item updated successfully";
                    response["success"] = true;
                    auto resp = HttpResponse::newHttpJsonResponse(response);
                    callback(resp);
                },
                [callback, this](const DrogonDbException& e) {
                    LOG_ERROR << "Database error: " << e.base().what();
                    auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                    resp->setStatusCode(k500InternalServerError);
                    callback(resp);
                },
                informationStr, userId
                );
        },
        [callback, this](const DrogonDbException& e) {
            LOG_ERROR << "Database error: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        userId
        );
}

void UserController::deleteInformationItem(const HttpRequestPtr& req,
                                           function<void(const HttpResponsePtr&)>&& callback,
                                           const string& infoId) {
    auto userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    dbClient->execSqlAsync(
        "SELECT information FROM users WHERE id = $1",
        [callback, userId, infoId, this](const Result& result) {
            if (result.empty()) {
                auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "User not found"));
                resp->setStatusCode(k404NotFound);
                callback(resp);
                return;
            }

            Json::Value informationArray;
            if (!result[0]["information"].isNull()) {
                string informationStr = result[0]["information"].as<string>();
                if (!informationStr.empty()) {
                    Json::Reader reader;
                    reader.parse(informationStr, informationArray);
                }
            }

            // Удаляем элемент
            Json::Value updatedArray = Json::arrayValue;
            for (const auto& item : informationArray) {
                if (item["id"].asString() != infoId) {
                    updatedArray.append(item);
                }
            }

            Json::FastWriter writer;
            string informationStr = writer.write(updatedArray);

            auto dbClient = app().getDbClient();
            dbClient->execSqlAsync(
                "UPDATE users SET information = $1, updated_at = NOW() WHERE id = $2",
                [callback](const Result& result) {
                    Json::Value response;
                    response["message"] = "Information item deleted successfully";
                    response["success"] = true;
                    auto resp = HttpResponse::newHttpJsonResponse(response);
                    callback(resp);
                },
                [callback, this](const DrogonDbException& e) {
                    LOG_ERROR << "Database error: " << e.base().what();
                    auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                    resp->setStatusCode(k500InternalServerError);
                    callback(resp);
                },
                informationStr, userId
                );
        },
        [callback, this](const DrogonDbException& e) {
            LOG_ERROR << "Database error: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        userId
        );
}

// Контакты (аналогично информации)
void UserController::getContacts(const HttpRequestPtr& req,
                                 function<void(const HttpResponsePtr&)>&& callback) {
    auto userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    dbClient->execSqlAsync(
        "SELECT contacts FROM users WHERE id = $1",
        [callback, this](const Result& result) {
            if (result.empty()) {
                auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "User not found"));
                resp->setStatusCode(k404NotFound);
                callback(resp);
                return;
            }

            Json::Value contacts;
            if (!result[0]["contacts"].isNull()) {
                string contactsStr = result[0]["contacts"].as<string>();
                if (!contactsStr.empty()) {
                    Json::Reader reader;
                    if (!reader.parse(contactsStr, contacts)) {
                        contacts = Json::arrayValue;
                    }
                } else {
                    contacts = Json::arrayValue;
                }
            } else {
                contacts = Json::arrayValue;
            }

            auto resp = HttpResponse::newHttpJsonResponse(contacts);
            callback(resp);
        },
        [callback, this](const DrogonDbException& e) {
            LOG_ERROR << "Database error: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        userId
        );
}

void UserController::addContact(const HttpRequestPtr& req,
                                function<void(const HttpResponsePtr&)>&& callback) {
    auto userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    auto json = req->getJsonObject();
    if (!json || !isValidContactItem(*json)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid contact item"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    dbClient->execSqlAsync(
        "SELECT contacts FROM users WHERE id = $1",
        [callback, userId, json, this](const Result& result) {
            if (result.empty()) {
                auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "User not found"));
                resp->setStatusCode(k404NotFound);
                callback(resp);
                return;
            }

            Json::Value contactsArray;
            if (!result[0]["contacts"].isNull()) {
                string contactsStr = result[0]["contacts"].as<string>();
                if (!contactsStr.empty()) {
                    Json::Reader reader;
                    reader.parse(contactsStr, contactsArray);
                }
            }

            string newId = utils::getUuid();
            (*json)["id"] = newId;
            contactsArray.append(*json);

            Json::FastWriter writer;
            string contactsStr = writer.write(contactsArray);

            auto dbClient = app().getDbClient();
            dbClient->execSqlAsync(
                "UPDATE users SET contacts = $1, updated_at = NOW() WHERE id = $2",
                [callback, newId](const Result& result) {
                    Json::Value response;
                    response["message"] = "Contact added successfully";
                    response["id"] = newId;
                    response["success"] = true;
                    auto resp = HttpResponse::newHttpJsonResponse(response);
                    callback(resp);
                },
                [callback, this](const DrogonDbException& e) {
                    LOG_ERROR << "Database error: " << e.base().what();
                    auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                    resp->setStatusCode(k500InternalServerError);
                    callback(resp);
                },
                contactsStr, userId
                );
        },
        [callback, this](const DrogonDbException& e) {
            LOG_ERROR << "Database error: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        userId
        );
}

void UserController::updateAllContacts(const HttpRequestPtr& req,
                                       function<void(const HttpResponsePtr&)>&& callback) {
    auto userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    auto json = req->getJsonObject();
    if (!json || !json->isArray()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid data format"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    for (const auto& item : *json) {
        if (!isValidContactItem(item)) {
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid contact item"));
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            return;
        }
    }

    Json::FastWriter writer;
    string contactsStr = writer.write(*json);

    auto dbClient = app().getDbClient();

    dbClient->execSqlAsync(
        "UPDATE users SET contacts = $1, updated_at = NOW() WHERE id = $2",
        [callback](const Result& result) {
            Json::Value response;
            response["message"] = "All contacts updated successfully";
            response["success"] = true;
            auto resp = HttpResponse::newHttpJsonResponse(response);
            callback(resp);
        },
        [callback, this](const DrogonDbException& e) {
            LOG_ERROR << "Database error: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        contactsStr, userId
        );
}

void UserController::updateContactItem(const HttpRequestPtr& req,
                                       function<void(const HttpResponsePtr&)>&& callback,
                                       const string& contactId) {
    auto userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    auto json = req->getJsonObject();
    if (!json || !isValidContactItem(*json)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid contact item"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    dbClient->execSqlAsync(
        "SELECT contacts FROM users WHERE id = $1",
        [callback, userId, contactId, json, this](const Result& result) {
            if (result.empty()) {
                auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "User not found"));
                resp->setStatusCode(k404NotFound);
                callback(resp);
                return;
            }

            Json::Value contactsArray;
            if (!result[0]["contacts"].isNull()) {
                string contactsStr = result[0]["contacts"].as<string>();
                if (!contactsStr.empty()) {
                    Json::Reader reader;
                    reader.parse(contactsStr, contactsArray);
                }
            }

            Json::Value updatedArray = Json::arrayValue;
            bool found = false;

            for (const auto& item : contactsArray) {
                if (item["id"].asString() == contactId) {
                    Json::Value updatedItem = *json;
                    updatedItem["id"] = contactId;
                    updatedArray.append(updatedItem);
                    found = true;
                } else {
                    updatedArray.append(item);
                }
            }

            if (!found) {
                Json::Value newItem = *json;
                newItem["id"] = contactId;
                updatedArray.append(newItem);
            }

            Json::FastWriter writer;
            string contactsStr = writer.write(updatedArray);

            auto dbClient = app().getDbClient();
            dbClient->execSqlAsync(
                "UPDATE users SET contacts = $1, updated_at = NOW() WHERE id = $2",
                [callback](const Result& result) {
                    Json::Value response;
                    response["message"] = "Contact item updated successfully";
                    response["success"] = true;
                    auto resp = HttpResponse::newHttpJsonResponse(response);
                    callback(resp);
                },
                [callback, this](const DrogonDbException& e) {
                    LOG_ERROR << "Database error: " << e.base().what();
                    auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                    resp->setStatusCode(k500InternalServerError);
                    callback(resp);
                },
                contactsStr, userId
                );
        },
        [callback, this](const DrogonDbException& e) {
            LOG_ERROR << "Database error: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        userId
        );
}

void UserController::deleteContactItem(const HttpRequestPtr& req,
                                       function<void(const HttpResponsePtr&)>&& callback,
                                       const string& contactId) {
    auto userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    dbClient->execSqlAsync(
        "SELECT contacts FROM users WHERE id = $1",
        [callback, userId, contactId, this](const Result& result) {
            if (result.empty()) {
                auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "User not found"));
                resp->setStatusCode(k404NotFound);
                callback(resp);
                return;
            }

            Json::Value contactsArray;
            if (!result[0]["contacts"].isNull()) {
                string contactsStr = result[0]["contacts"].as<string>();
                if (!contactsStr.empty()) {
                    Json::Reader reader;
                    reader.parse(contactsStr, contactsArray);
                }
            }

            Json::Value updatedArray = Json::arrayValue;
            for (const auto& item : contactsArray) {
                if (item["id"].asString() != contactId) {
                    updatedArray.append(item);
                }
            }

            Json::FastWriter writer;
            string contactsStr = writer.write(updatedArray);

            auto dbClient = app().getDbClient();
            dbClient->execSqlAsync(
                "UPDATE users SET contacts = $1, updated_at = NOW() WHERE id = $2",
                [callback](const Result& result) {
                    Json::Value response;
                    response["message"] = "Contact item deleted successfully";
                    response["success"] = true;
                    auto resp = HttpResponse::newHttpJsonResponse(response);
                    callback(resp);
                },
                [callback, this](const DrogonDbException& e) {
                    LOG_ERROR << "Database error: " << e.base().what();
                    auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                    resp->setStatusCode(k500InternalServerError);
                    callback(resp);
                },
                contactsStr, userId
                );
        },
        [callback, this](const DrogonDbException& e) {
            LOG_ERROR << "Database error: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        userId
        );
}

// Прогресс обучения
void UserController::getAllCoursesProgress(const HttpRequestPtr& req,
                                           function<void(const HttpResponsePtr&)>&& callback) {
    auto userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    dbClient->execSqlAsync(
        "SELECT ce.course_id, c.title as course_title, ce.completion_percentage, "
        "ce.is_completed, ce.last_accessed_at, "
        "(SELECT COUNT(*) FROM user_progress up "
        "JOIN course_videos cv ON up.video_id = cv.id "
        "WHERE up.user_id = ce.user_id AND up.course_id = ce.course_id AND up.completed = true AND cv.is_approved = true) as videos_completed, "
        "(SELECT COUNT(*) FROM course_videos WHERE course_id = ce.course_id AND is_approved = true) as total_videos "
        "FROM course_enrollments ce "
        "JOIN courses c ON ce.course_id = c.id "
        "WHERE ce.user_id = $1",
        [callback](const Result& result) {
            Json::Value progressArray(Json::arrayValue);

            for (const auto& row : result) {
                Json::Value progress;
                progress["course_id"] = row["course_id"].as<string>();
                progress["course_title"] = row["course_title"].as<string>();
                progress["completion_percentage"] = row["completion_percentage"].as<int>();
                progress["is_completed"] = row["is_completed"].as<bool>();
                progress["last_accessed_at"] = row["last_accessed_at"].as<string>();
                progress["videos_completed"] = row["videos_completed"].as<int>();
                progress["total_videos"] = row["total_videos"].as<int>();

                progressArray.append(progress);
            }

            auto resp = HttpResponse::newHttpJsonResponse(progressArray);
            callback(resp);
        },
        [callback, this](const DrogonDbException& e) {
            LOG_ERROR << "Database error: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        userId
        );
}

void UserController::getCourseProgress(const HttpRequestPtr& req,
                                       function<void(const HttpResponsePtr&)>&& callback,
                                       const string& courseId) {
    auto userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();

    // Сначала проверяем существует ли курс и запись о прогрессе
    dbClient->execSqlAsync(
        "SELECT 1 FROM course_enrollments WHERE user_id = $1 AND course_id = $2",
        [callback, userId, courseId, dbClient, this](const Result& checkResult) {
            if (checkResult.empty()) {
                auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Course not found or not enrolled"));
                resp->setStatusCode(k404NotFound);
                callback(resp);
                return;
            }

            // Если курс существует, получаем прогресс
            dbClient->execSqlAsync(
                "SELECT up.video_id, cv.title as video_title, up.completed, "
                "up.watched_seconds, up.total_seconds, up.progress_percentage, "
                "up.last_watched_at, cv.duration, cv.duration_seconds "
                "FROM user_progress up "
                "JOIN course_videos cv ON up.video_id = cv.id "
                "WHERE up.user_id = $1 AND up.course_id = $2 AND cv.is_approved = true",
                [callback, courseId, userId](const Result& result) {
                    Json::Value progress;
                    progress["course_id"] = courseId;
                    progress["user_id"] = userId;
                    Json::Value videosProgress(Json::arrayValue);

                    for (const auto& row : result) {
                        Json::Value videoProgress;
                        videoProgress["video_id"] = row["video_id"].as<string>();
                        videoProgress["video_title"] = row["video_title"].as<string>();
                        videoProgress["completed"] = row["completed"].as<bool>();
                        videoProgress["watched_seconds"] = row["watched_seconds"].as<int>();
                        videoProgress["total_seconds"] = row["total_seconds"].as<int>();
                        videoProgress["progress_percentage"] = row["progress_percentage"].as<int>();
                        videoProgress["last_watched_at"] = row["last_watched_at"].as<string>();
                        videoProgress["duration"] = row["duration"].as<string>();
                        videoProgress["duration_seconds"] = row["duration_seconds"].as<int>();

                        videosProgress.append(videoProgress);
                    }

                    progress["videos"] = videosProgress;

                    // Если нет записей о прогрессе, возвращаем пустой массив с сообщением
                    if (videosProgress.empty()) {
                        progress["message"] = "No progress data found for this course";
                    }

                    auto resp = HttpResponse::newHttpJsonResponse(progress);
                    callback(resp);
                },
                [callback, this](const DrogonDbException& e) {
                    LOG_ERROR << "Database error: " << e.base().what();
                    auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
                    resp->setStatusCode(k500InternalServerError);
                    callback(resp);
                },
                userId, courseId
                );
        },
        [callback, this](const DrogonDbException& e) {
            LOG_ERROR << "Database error: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        userId, courseId
        );
}

void UserController::updateLessonProgress(const HttpRequestPtr& req,
                                          function<void(const HttpResponsePtr&)>&& callback) {
    auto userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    auto json = req->getJsonObject();
    if (!json || !isValidProgressData(*json)) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid progress data"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    string videoId = (*json)["video_id"].asString();
    bool completed = (*json)["completed"].asBool();
    int watchedSeconds = (*json)["watched_seconds"].asInt();

    auto dbClient = app().getDbClient();

    dbClient->execSqlAsync(
        "INSERT INTO user_progress (user_id, course_id, video_id, completed, watched_seconds, progress_percentage, last_watched_at) "
        "VALUES ($1, (SELECT course_id FROM course_videos WHERE id = $2), $2, $3, $4, "
        "LEAST(($4 * 100) / NULLIF((SELECT duration_seconds FROM course_videos WHERE id = $2), 0), 100), NOW()) "
        "ON CONFLICT (user_id, video_id) DO UPDATE SET "
        "completed = $3, watched_seconds = $4, progress_percentage = "
        "LEAST(($4 * 100) / NULLIF((SELECT duration_seconds FROM course_videos WHERE id = $2), 0), 100), "
        "last_watched_at = NOW(), updated_at = NOW()",
        [callback, videoId](const Result& result) {
            Json::Value response;
            response["message"] = "Lesson progress updated successfully";
            response["success"] = true;
            auto resp = HttpResponse::newHttpJsonResponse(response);
            callback(resp);
        },
        [callback, this](const DrogonDbException& e) {
            LOG_ERROR << "Database error: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        userId, videoId, completed, watchedSeconds
        );
}

void UserController::updateLessonProgressDuration(const HttpRequestPtr& req,
                                                  function<void(const HttpResponsePtr&)>&& callback,
                                                  const string& lessonId) {
    auto userId = getCurrentUserId(req);
    if (userId.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Unauthorized"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    auto json = req->getJsonObject();
    if (!json || !json->isMember("watched_seconds")) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid data"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    int watchedSeconds = (*json)["watched_seconds"].asInt();

    auto dbClient = app().getDbClient();

    dbClient->execSqlAsync(
        "UPDATE user_progress SET watched_seconds = $1, "
        "progress_percentage = LEAST(($1 * 100) / NULLIF((SELECT duration_seconds FROM course_videos WHERE id = $2), 0), 100), "
        "last_watched_at = NOW(), updated_at = NOW() "
        "WHERE user_id = $3 AND video_id = $2",
        [callback, lessonId](const Result& result) {
            Json::Value response;
            response["message"] = "Lesson progress duration updated successfully";
            response["success"] = true;
            auto resp = HttpResponse::newHttpJsonResponse(response);
            callback(resp);
        },
        [callback, this](const DrogonDbException& e) {
            LOG_ERROR << "Database error: " << e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Database error"));
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        watchedSeconds, lessonId, userId
        );
}
