#include "AuthController.h"
#include <drogon/drogon.h>
#include <bcrypt/BCrypt.hpp>
#include <jwt-cpp/jwt.h>
#include <random>
#include <curl/curl.h>

using namespace drogon;
using namespace drogon_model::myserver;

// Статические константы
const std::string AuthController::JWT_SECRET = "your-super-secret-jwt-key-change-in-production";
const std::string AuthController::GOOGLE_CLIENT_ID = "your-google-client-id";

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t totalSize = size * nmemb;
    response->append((char*)contents, totalSize);
    return totalSize;
}

Json::Value AuthController::createJsonResponse(const std::string& key, const std::string& value) {
    Json::Value json;
    json[key] = value;
    return json;
}

std::string AuthController::generateRandomPassword() {
    const std::string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(0, chars.size() - 1);

    std::string password;
    for (int i = 0; i < 16; ++i) {
        password += chars[distribution(generator)];
    }
    return password;
}

Json::Value AuthController::verifyGoogleToken(const std::string& token) {
    CURL* curl;
    CURLcode res;
    std::string response_string;

    curl = curl_easy_init();
    if(curl) {
        std::string url = "https://www.googleapis.com/oauth2/v3/tokeninfo?id_token=" + token;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
            LOG_ERROR << "Google OAuth request failed: " << curl_easy_strerror(res);
            curl_easy_cleanup(curl);
            return Json::Value();
        }

        curl_easy_cleanup(curl);

        Json::Value googleData;
        Json::Reader reader;
        if (reader.parse(response_string, googleData)) {
            if (googleData["aud"].asString() != GOOGLE_CLIENT_ID) {
                LOG_ERROR << "Google OAuth audience mismatch";
                return Json::Value();
            }

            return googleData;
        } else {
            LOG_ERROR << "Failed to parse Google OAuth response";
            return Json::Value();
        }
    }

    return Json::Value();
}

void AuthController::asyncHandleHttpRequest(const HttpRequestPtr &req,
                                            std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto path = req->getPath();

    if (path == "/auth/register") {
        handleRegister(req, std::move(callback));
    } else if (path == "/auth/login") {
        handleLogin(req, std::move(callback));
    } else if (path == "/auth/google") {
        handleGoogleAuth(req, std::move(callback));
    } else if (path == "/auth/refresh") {
        handleRefreshToken(req, std::move(callback));
    } else if (path == "/auth/logout") {
        handleLogout(req, std::move(callback));
    } else {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Not found"));
        resp->setStatusCode(k404NotFound);
        callback(resp);
    }
}

void AuthController::handleRegister(const HttpRequestPtr &req,
                                    std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto json = req->getJsonObject();
    if (!json) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid JSON"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    std::string username = json->get("username", "").asString();
    std::string email = json->get("email", "").asString();
    std::string password = json->get("password", "").asString();

    if (username.empty() || email.empty() || password.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "All fields are required"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    if (password.length() < 6) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Password must be at least 6 characters"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto client = drogon::app().getDbClient();
    try {
        auto result = client->execSqlSync("SELECT id FROM users WHERE email = $1 OR username = $2", email, username);
        if (!result.empty()) {
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "User with this email or username already exists"));
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            return;
        }

        std::string id = drogon::utils::getUuid();
        std::string passwordHash = BCrypt::generateHash(password);

        LOG_DEBUG << "Registering user - ID: " << id << ", Username: " << username << ", Email: " << email;

        // Вставляем пользователя с учетом структуры базы данных
        client->execSqlSync(
            "INSERT INTO users (id, username, email, password_hash, role, profile_is_public, last_login_at, contacts, information) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)",
            id, username, email, passwordHash, "пользователь", true,
            trantor::Date::date(), "[]", "[]"
            );

        LOG_DEBUG << "Insert completed, now selecting user...";

        // Получаем созданного пользователя
        auto userResult = client->execSqlSync("SELECT * FROM users WHERE id = $1", id);
        if (userResult.empty()) {
            LOG_ERROR << "User not found after insertion. ID: " << id;
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Failed to create user"));
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
            return;
        }

        LOG_DEBUG << "User found, creating response...";

        auto user = Users(userResult[0]);
        auto userJson = getUserResponse(user);

        // Добавим отладочный вывод
        LOG_DEBUG << "User JSON prepared - ID: " << userJson["id"].asString()
                  << ", Username: " << userJson["username"].asString()
                  << ", Email: " << userJson["email"].asString();

        std::string token = generateJWT(userJson);

        Json::Value refreshPayload;
        refreshPayload["user_id"] = id;
        refreshPayload["type"] = "refresh";
        std::string refreshToken = generateJWT(refreshPayload);

        Json::Value response;
        response["message"] = "User registered successfully";
        response["user"] = userJson;
        response["token"] = token;
        response["refresh_token"] = refreshToken;

        auto resp = HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const std::exception &e) {
        LOG_ERROR << "Registration error: " << e.what();
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Internal server error"));
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
    }
}

void AuthController::handleLogin(const HttpRequestPtr &req,
                                 std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto json = req->getJsonObject();
    if (!json) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid JSON"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    // Принимаем либо email, либо username для входа
    std::string email = json->get("email", "").asString();
    std::string username = json->get("username", "").asString();
    std::string password = json->get("password", "").asString();

    if (password.empty() || (email.empty() && username.empty())) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Email/username and password are required"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto client = drogon::app().getDbClient();
    try {
        // Ищем пользователя по email ИЛИ username
        auto result = email.empty()
                          ? client->execSqlSync("SELECT * FROM users WHERE username = $1", username)
                          : client->execSqlSync("SELECT * FROM users WHERE email = $1", email);

        if (result.empty()) {
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid email/username or password"));
            resp->setStatusCode(k401Unauthorized);
            callback(resp);
            return;
        }

        auto user = Users(result[0]);

        // Получаем парольный хэш
        std::string passwordHash;
        try {
            passwordHash = user.getValueOfPasswordHash();
        } catch (const std::exception &e) {
            LOG_ERROR << "Error getting password hash: " << e.what();
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid email/username or password"));
            resp->setStatusCode(k401Unauthorized);
            callback(resp);
            return;
        }

        if (!BCrypt::validatePassword(password, passwordHash)) {
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid email/username or password"));
            resp->setStatusCode(k401Unauthorized);
            callback(resp);
            return;
        }

        // Обновляем last_login_at
        client->execSqlSync("UPDATE users SET last_login_at = $1 WHERE id = $2",
                            trantor::Date::date(), user.getValueOfId());

        auto userJson = getUserResponse(user);
        std::string token = generateJWT(userJson);

        Json::Value refreshPayload;
        refreshPayload["user_id"] = user.getValueOfId();
        refreshPayload["type"] = "refresh";
        std::string refreshToken = generateJWT(refreshPayload);

        Json::Value response;
        response["message"] = "Login successful";
        response["user"] = userJson;
        response["token"] = token;
        response["refresh_token"] = refreshToken;

        auto resp = HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const std::exception &e) {
        LOG_ERROR << "Login error: " << e.what();
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Internal server error"));
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
    }
}

void AuthController::handleGoogleAuth(const HttpRequestPtr &req,
                                      std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto json = req->getJsonObject();
    if (!json) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid JSON"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    std::string googleToken = json->get("token", "").asString();
    if (googleToken.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Google token is required"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    try {
        Json::Value googleData = verifyGoogleToken(googleToken);
        if (googleData.isNull()) {
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid Google token"));
            resp->setStatusCode(k401Unauthorized);
            callback(resp);
            return;
        }

        std::string email = googleData["email"].asString();
        std::string name = googleData["name"].asString();
        std::string picture = googleData["picture"].asString();
        std::string givenName = googleData["given_name"].asString();

        if (email.empty()) {
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Email not provided by Google"));
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            return;
        }

        auto client = drogon::app().getDbClient();
        auto result = client->execSqlSync("SELECT * FROM users WHERE email = $1", email);

        Users user;
        bool isNewUser = false;

        if (result.empty()) {
            isNewUser = true;
            std::string id = drogon::utils::getUuid();
            std::string randomPassword = generateRandomPassword();
            std::string passwordHash = BCrypt::generateHash(randomPassword);

            std::string username = givenName.empty() ? email.substr(0, email.find('@')) : givenName;

            // Генерируем уникальное имя пользователя
            int counter = 1;
            std::string originalUsername = username;
            while (true) {
                auto usernameCheck = client->execSqlSync("SELECT id FROM users WHERE username = $1", username);
                if (usernameCheck.empty()) break;
                username = originalUsername + std::to_string(counter++);
            }

            // Вставляем пользователя с учетом структуры базы данных
            client->execSqlSync(
                "INSERT INTO users (id, username, email, password_hash, role, profile_is_public, last_login_at, avatar_path, contacts, information) "
                "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10)",
                id, username, email, passwordHash, "пользователь", true,
                trantor::Date::date(), picture, "[]", "[]"
                );

            // Получаем созданного пользователя
            auto userResult = client->execSqlSync("SELECT * FROM users WHERE id = $1", id);
            if (userResult.empty()) {
                auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Failed to create user via Google"));
                resp->setStatusCode(k500InternalServerError);
                callback(resp);
                return;
            }
            user = Users(userResult[0]);
        } else {
            user = Users(result[0]);
            // Обновляем last_login_at для существующего пользователя
            client->execSqlSync("UPDATE users SET last_login_at = $1 WHERE id = $2",
                                trantor::Date::date(), user.getValueOfId());
        }

        auto userJson = getUserResponse(user);
        std::string token = generateJWT(userJson);

        Json::Value refreshPayload;
        refreshPayload["user_id"] = user.getValueOfId();
        refreshPayload["type"] = "refresh";
        std::string refreshToken = generateJWT(refreshPayload);

        Json::Value response;
        response["message"] = isNewUser ? "User registered via Google successfully" : "Google login successful";
        response["user"] = userJson;
        response["token"] = token;
        response["refresh_token"] = refreshToken;

        auto resp = HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const std::exception &e) {
        LOG_ERROR << "Google OAuth error: " << e.what();
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Google OAuth failed"));
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
    }
}

void AuthController::handleRefreshToken(const HttpRequestPtr &req,
                                        std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto json = req->getJsonObject();
    if (!json) {
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid JSON"));
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    std::string refreshToken = json->get("refresh_token", "").asString();

    try {
        auto decoded = validateJWT(refreshToken);
        if (decoded.isNull() || decoded["type"].asString() != "refresh") {
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid refresh token"));
            resp->setStatusCode(k401Unauthorized);
            callback(resp);
            return;
        }

        std::string userId = decoded["user_id"].asString();
        auto client = drogon::app().getDbClient();
        auto result = client->execSqlSync("SELECT * FROM users WHERE id = $1", userId);

        if (result.empty()) {
            auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "User not found"));
            resp->setStatusCode(k401Unauthorized);
            callback(resp);
            return;
        }

        auto user = Users(result[0]);
        auto userJson = getUserResponse(user);
        std::string newToken = generateJWT(userJson);

        Json::Value response;
        response["token"] = newToken;
        response["user"] = userJson;

        auto resp = HttpResponse::newHttpJsonResponse(response);
        callback(resp);

    } catch (const std::exception &e) {
        LOG_ERROR << "Token refresh error: " << e.what();
        auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("error", "Invalid refresh token"));
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
    }
}

void AuthController::handleLogout(const HttpRequestPtr &req,
                                  std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto resp = HttpResponse::newHttpJsonResponse(createJsonResponse("message", "Logout successful"));
    callback(resp);
}

std::string AuthController::generateJWT(const Json::Value &user)
{
    try {
        auto builder = jwt::create()
        .set_issuer("auth-server")
            .set_type("JWS")
            .set_issued_at(std::chrono::system_clock::now())
            .set_expires_at(std::chrono::system_clock::now() + std::chrono::hours{24});

        if (user.isMember("id")) {
            builder.set_payload_claim("user_id", jwt::claim(user["id"].asString()));
        }
        if (user.isMember("email")) {
            builder.set_payload_claim("email", jwt::claim(user["email"].asString()));
        }
        if (user.isMember("role")) {
            builder.set_payload_claim("role", jwt::claim(user["role"].asString()));
        }
        if (user.isMember("type")) {
            builder.set_payload_claim("type", jwt::claim(user["type"].asString()));
        }
        if (user.isMember("user_id")) {
            builder.set_payload_claim("user_id", jwt::claim(user["user_id"].asString()));
        }

        return builder.sign(jwt::algorithm::hs256{JWT_SECRET});
    } catch (const std::exception &e) {
        LOG_ERROR << "JWT generation error: " << e.what();
        return "";
    }
}

Json::Value AuthController::validateJWT(const std::string &token)
{
    try {
        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify()
                            .allow_algorithm(jwt::algorithm::hs256{JWT_SECRET})
                            .with_issuer("auth-server");

        verifier.verify(decoded);

        Json::Value payload;

        try {
            auto user_id_claim = decoded.get_payload_claim("user_id");
            payload["user_id"] = user_id_claim.as_string();
        } catch (const std::exception& e) {
            LOG_DEBUG << "user_id claim not found or invalid: " << e.what();
        }

        try {
            auto email_claim = decoded.get_payload_claim("email");
            payload["email"] = email_claim.as_string();
        } catch (const std::exception& e) {
            LOG_DEBUG << "email claim not found or invalid: " << e.what();
        }

        try {
            auto role_claim = decoded.get_payload_claim("role");
            payload["role"] = role_claim.as_string();
        } catch (const std::exception& e) {
            LOG_DEBUG << "role claim not found or invalid: " << e.what();
        }

        try {
            auto type_claim = decoded.get_payload_claim("type");
            payload["type"] = type_claim.as_string();
        } catch (const std::exception& e) {
            LOG_DEBUG << "type claim not found or invalid: " << e.what();
        }

        return payload;
    } catch (const std::exception &e) {
        LOG_ERROR << "JWT validation error: " << e.what();
        return Json::Value();
    }
}

std::string AuthController::hashPassword(const std::string &password)
{
    try {
        return BCrypt::generateHash(password);
    } catch (const std::exception &e) {
        LOG_ERROR << "Password hashing error: " << e.what();
        return "";
    }
}

bool AuthController::verifyPassword(const std::string &password, const std::string &hash)
{
    try {
        return BCrypt::validatePassword(password, hash);
    } catch (const std::exception &e) {
        LOG_ERROR << "Password verification error: " << e.what();
        return false;
    }
}

Json::Value AuthController::getUserResponse(const drogon_model::myserver::Users &user)
{
    Json::Value userJson;

    // Обязательные поля
    userJson["id"] = user.getValueOfId();
    userJson["username"] = user.getValueOfUsername();
    userJson["email"] = user.getValueOfEmail();
    userJson["role"] = user.getValueOfRole();
    userJson["profile_is_public"] = user.getValueOfProfileIsPublic();

    // Опциональные поля - используем безопасный подход
    try {
        std::string avatarPath = user.getValueOfAvatarPath();
        if (!avatarPath.empty()) {
            userJson["avatar_path"] = avatarPath;
        } else {
            userJson["avatar_path"] = Json::nullValue;
        }
    } catch (const std::exception& e) {
        userJson["avatar_path"] = Json::nullValue;
    }

    try {
        std::string coverPath = user.getValueOfCoverPath();
        if (!coverPath.empty()) {
            userJson["cover_path"] = coverPath;
        } else {
            userJson["cover_path"] = Json::nullValue;
        }
    } catch (const std::exception& e) {
        userJson["cover_path"] = Json::nullValue;
    }

    // Поля contacts и information
    try {
        std::string contacts = user.getValueOfContacts();
        if (!contacts.empty()) {
            Json::Value contactsJson;
            Json::Reader reader;
            if (reader.parse(contacts, contactsJson)) {
                userJson["contacts"] = contactsJson;
            } else {
                userJson["contacts"] = Json::arrayValue;
            }
        } else {
            userJson["contacts"] = Json::arrayValue;
        }
    } catch (const std::exception& e) {
        userJson["contacts"] = Json::arrayValue;
    }

    try {
        std::string information = user.getValueOfInformation();
        if (!information.empty()) {
            Json::Value infoJson;
            Json::Reader reader;
            if (reader.parse(information, infoJson)) {
                userJson["information"] = infoJson;
            } else {
                userJson["information"] = Json::arrayValue;
            }
        } else {
            userJson["information"] = Json::arrayValue;
        }
    } catch (const std::exception& e) {
        userJson["information"] = Json::arrayValue;
    }

    // Даты
    userJson["created_at"] = user.getValueOfCreatedAt().toDbStringLocal();
    userJson["updated_at"] = user.getValueOfUpdatedAt().toDbStringLocal();

    // Проверяем last_login_at
    try {
        trantor::Date lastLogin = user.getValueOfLastLoginAt();
        if (lastLogin != trantor::Date()) {
            userJson["last_login_at"] = lastLogin.toDbStringLocal();
        } else {
            userJson["last_login_at"] = Json::nullValue;
        }
    } catch (const std::exception& e) {
        userJson["last_login_at"] = Json::nullValue;
    }

    return userJson;
}
