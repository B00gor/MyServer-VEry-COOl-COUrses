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

using namespace drogon;
using namespace drogon_model::myserver;

class ChannelController : public drogon::HttpController<ChannelController>
{
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(ChannelController::getChannelInfo, "/channels/{1}", Get);
    ADD_METHOD_TO(ChannelController::getChannelCourses, "/channels/{1}/courses", Get);
    METHOD_LIST_END

        // Получить информацию о канале
        void getChannelInfo(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback,
                       const std::string& channelId);

    // Получить курсы канала
    void getChannelCourses(const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& callback,
                           const std::string& channelId);

private:
    // Вспомогательные методы
    Json::Value createJsonResponse(const std::string& key, const std::string& value);
    Json::Value getUserJsonResponse(const Users& user);
    Json::Value getCourseJsonResponse(const Courses& course);

    // Валидация
    bool isValidUUID(const std::string& uuid);

    // JWT аутентификация
    std::string getCurrentUserId(const HttpRequestPtr& req);
    bool validateJWT(const std::string& token);
    std::string getUserIdFromToken(const std::string& token);
    std::string getRoleFromToken(const std::string& token);
    bool hasPermission(const HttpRequestPtr& req, const std::vector<std::string>& allowedRoles);

    const std::string JWT_SECRET = "your-super-secret-jwt-key-change-in-production";
};
