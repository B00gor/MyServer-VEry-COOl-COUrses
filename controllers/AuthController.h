#ifndef AUTHCONTROLLER_H
#define AUTHCONTROLLER_H

#include <drogon/HttpSimpleController.h>
#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>
#include <json/json.h>
#include "../models/Users.h"

using namespace drogon;

class AuthController : public drogon::HttpSimpleController<AuthController>
{
public:
    void asyncHandleHttpRequest(const HttpRequestPtr &req,
                               std::function<void(const HttpResponsePtr &)> &&callback) override;

    PATH_LIST_BEGIN
    PATH_ADD("/auth/register", Post);
    PATH_ADD("/auth/login", Post);
    PATH_ADD("/auth/google", Post);
    PATH_ADD("/auth/refresh", Post);
    PATH_ADD("/auth/logout", Post);
    PATH_LIST_END

private:
    static const std::string JWT_SECRET;
    static const std::string GOOGLE_CLIENT_ID;

    void handleRegister(const HttpRequestPtr &req,
                       std::function<void(const HttpResponsePtr &)> &&callback);
    void handleLogin(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);
    void handleGoogleAuth(const HttpRequestPtr &req,
                         std::function<void(const HttpResponsePtr &)> &&callback);
    void handleRefreshToken(const HttpRequestPtr &req,
                           std::function<void(const HttpResponsePtr &)> &&callback);
    void handleLogout(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);

    Json::Value createJsonResponse(const std::string &key, const std::string &value);
    std::string generateRandomPassword();
    Json::Value verifyGoogleToken(const std::string &token);
    std::string generateJWT(const Json::Value &user);
    Json::Value validateJWT(const std::string &token);
    std::string hashPassword(const std::string &password);
    bool verifyPassword(const std::string &password, const std::string &hash);
    Json::Value getUserResponse(const drogon_model::myserver::Users &user);
};

#endif // AUTHCONTROLLER_H
