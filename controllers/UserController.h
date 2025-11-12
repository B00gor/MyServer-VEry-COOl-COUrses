#pragma once

#include <drogon/HttpSimpleController.h>
#include <drogon/HttpController.h>
#include <drogon/orm/DbClient.h>
#include <json/json.h>
#include <jwt-cpp/jwt.h>
#include "../models/Users.h"
#include "../models/UserStats.h"
#include "../models/Courses.h"
#include "../models/CourseVideos.h"
#include "../models/UserProgress.h"
#include "../models/CourseEnrollments.h"

using namespace drogon;
using namespace drogon_model::myserver;

class UserController : public drogon::HttpController<UserController>
{
public:
    METHOD_LIST_BEGIN
        // Основные операции с профилем
        ADD_METHOD_TO(UserController::getMyProfile, "/users/me", Get);
    ADD_METHOD_TO(UserController::updateMyProfile, "/users/me", Put);
    ADD_METHOD_TO(UserController::changeUserRole, "/users/{1}/role", Put);

    // Аватар
    ADD_METHOD_TO(UserController::uploadAvatar, "/users/me/avatar", Post);
    ADD_METHOD_TO(UserController::updateAvatar, "/users/me/avatar", Put);
    ADD_METHOD_TO(UserController::deleteAvatar, "/users/me/avatar", Delete);

    // Обложка
    ADD_METHOD_TO(UserController::uploadCover, "/users/me/cover", Post);
    ADD_METHOD_TO(UserController::updateCover, "/users/me/cover", Put);
    ADD_METHOD_TO(UserController::deleteCover, "/users/me/cover", Delete);

    // Информация
    ADD_METHOD_TO(UserController::getInformation, "/users/me/information", Get);
    ADD_METHOD_TO(UserController::addInformation, "/users/me/information", Post);
    ADD_METHOD_TO(UserController::updateAllInformation, "/users/me/information", Put);
    ADD_METHOD_TO(UserController::updateInformationItem, "/users/me/information/{1}", Put);
    ADD_METHOD_TO(UserController::deleteInformationItem, "/users/me/information/{1}", Delete);

    // Контакты
    ADD_METHOD_TO(UserController::getContacts, "/users/me/contacts", Get);
    ADD_METHOD_TO(UserController::addContact, "/users/me/contacts", Post);
    ADD_METHOD_TO(UserController::updateAllContacts, "/users/me/contacts", Put);
    ADD_METHOD_TO(UserController::updateContactItem, "/users/me/contacts/{1}", Put);
    ADD_METHOD_TO(UserController::deleteContactItem, "/users/me/contacts/{1}", Delete);

    // Прогресс обучения
    ADD_METHOD_TO(UserController::getAllCoursesProgress, "/users/me/progress/courses", Get);
    ADD_METHOD_TO(UserController::getCourseProgress, "/users/me/progress/courses/{1}", Get);
    ADD_METHOD_TO(UserController::updateLessonProgress, "/users/me/progress/lessons", Post);
    ADD_METHOD_TO(UserController::updateLessonProgressDuration, "/users/me/progress/lessons/{1}", Put);
    METHOD_LIST_END

        // Основные операции с профилем
        void getMyProfile(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback);
    void updateMyProfile(const HttpRequestPtr& req,
                         std::function<void(const HttpResponsePtr&)>&& callback);
    void changeUserRole(const HttpRequestPtr& req,
                        std::function<void(const HttpResponsePtr&)>&& callback,
                        const std::string& userId);

    // Аватар
    void uploadAvatar(const HttpRequestPtr& req,
                      std::function<void(const HttpResponsePtr&)>&& callback);
    void updateAvatar(const HttpRequestPtr& req,
                      std::function<void(const HttpResponsePtr&)>&& callback);
    void deleteAvatar(const HttpRequestPtr& req,
                      std::function<void(const HttpResponsePtr&)>&& callback);

    // Обложка
    void uploadCover(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback);
    void updateCover(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback);
    void deleteCover(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback);

    // Информация
    void getInformation(const HttpRequestPtr& req,
                        std::function<void(const HttpResponsePtr&)>&& callback);
    void addInformation(const HttpRequestPtr& req,
                        std::function<void(const HttpResponsePtr&)>&& callback);
    void updateAllInformation(const HttpRequestPtr& req,
                              std::function<void(const HttpResponsePtr&)>&& callback);
    void updateInformationItem(const HttpRequestPtr& req,
                               std::function<void(const HttpResponsePtr&)>&& callback,
                               const std::string& infoId);
    void deleteInformationItem(const HttpRequestPtr& req,
                               std::function<void(const HttpResponsePtr&)>&& callback,
                               const std::string& infoId);

    // Контакты
    void getContacts(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback);
    void addContact(const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback);
    void updateAllContacts(const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& callback);
    void updateContactItem(const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& callback,
                           const std::string& contactId);
    void deleteContactItem(const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& callback,
                           const std::string& contactId);

    // Прогресс обучения
    void getAllCoursesProgress(const HttpRequestPtr& req,
                               std::function<void(const HttpResponsePtr&)>&& callback);
    void getCourseProgress(const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& callback,
                           const std::string& courseId);
    void updateLessonProgress(const HttpRequestPtr& req,
                              std::function<void(const HttpResponsePtr&)>&& callback);
    void updateLessonProgressDuration(const HttpRequestPtr& req,
                                      std::function<void(const HttpResponsePtr&)>&& callback,
                                      const std::string& lessonId);

private:
    // JWT аутентификация
    std::string getCurrentUserId(const HttpRequestPtr& req);
    bool validateJWT(const std::string& token);
    std::string getUserIdFromToken(const std::string& token);
    std::string getRoleFromToken(const std::string& token);
    bool hasPermission(const HttpRequestPtr& req, const std::vector<std::string>& allowedRoles);

    // Валидация
    bool isValidUserRole(const std::string& role);
    bool isValidInformationItem(const Json::Value& item);
    bool isValidContactItem(const Json::Value& item);
    bool isValidProgressData(const Json::Value& data);

    // Работа с файлами
    std::string saveUploadedFile(const HttpRequestPtr& req,
                                 const std::string& uploadPath);
    bool deleteFile(const std::string& filePath);

    // JSON обработка
    Json::Value updateJsonArray(const Json::Value& originalArray,
                                const Json::Value& newItem,
                                const std::string& itemId = "");
    Json::Value removeFromJsonArray(const Json::Value& originalArray,
                                    const std::string& itemId);

    // Вспомогательные методы
    Json::Value createJsonResponse(const std::string& key, const std::string& value);
    Json::Value getUserJsonResponse(const Users& user);

    const std::string JWT_SECRET = "your-super-secret-jwt-key-change-in-production";
};
