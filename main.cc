#include "controllers/UserController.h"
#include <drogon/drogon.h>
#include <filesystem>
#include <string>

int main() {
    drogon::app().loadConfigFile("../../config/config.json");
    drogon::app().registerHandler("/test",
                                  [](const drogon::HttpRequestPtr &req,
                                     std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
                                      auto resp = drogon::HttpResponse::newHttpResponse();
                                      resp->setBody("Server is working!");
                                      callback(resp);
                                  });
    drogon::app().run();
    return 0;
}
