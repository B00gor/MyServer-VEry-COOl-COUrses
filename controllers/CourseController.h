#pragma once

#include <drogon/HttpSimpleController.h>
#include <drogon/HttpController.h>
#include <drogon/orm/DbClient.h>
#include <json/json.h>
#include <jwt-cpp/jwt.h>
#include "../models/Users.h"
#include "../models/Courses.h"
#include "../models/CourseChapters.h"
#include "../models/CourseVideos.h"
#include "../models/CourseEnrollments.h"
#include "../models/UserProgress.h"
#include <filesystem>
#include <fstream>

using namespace drogon;
using namespace drogon_model::myserver;
namespace fs = std::filesystem;

class CourseController : public drogon::HttpController<CourseController>
{
public:
    METHOD_LIST_BEGIN
        // Основные методы курсов
        ADD_METHOD_TO(CourseController::getCourses, "/courses", Get);
    ADD_METHOD_TO(CourseController::createCourse, "/courses", Post);
    ADD_METHOD_TO(CourseController::getCourseById, "/courses/{1}", Get);
    ADD_METHOD_TO(CourseController::updateCourse, "/courses/{1}", Put);
    ADD_METHOD_TO(CourseController::deleteCourse, "/courses/{1}", Delete);

    // Структура курса
    ADD_METHOD_TO(CourseController::getCourseStructure, "/courses/{1}/structure", Get);

    // Управление главами
    ADD_METHOD_TO(CourseController::getChapters, "/courses/{1}/chapters", Get);
    ADD_METHOD_TO(CourseController::createChapter, "/courses/{1}/chapters", Post);
    ADD_METHOD_TO(CourseController::updateChaptersOrder, "/courses/{1}/chapters/order", Put);
    ADD_METHOD_TO(CourseController::updateChapter, "/courses/{1}/chapters/{2}", Put);
    ADD_METHOD_TO(CourseController::deleteChapter, "/courses/{1}/chapters/{2}", Delete);

    // Управление видео
    ADD_METHOD_TO(CourseController::getVideos, "/courses/{1}/videos", Get);
    ADD_METHOD_TO(CourseController::createVideoInCourse, "/courses/{1}/videos", Post);
    ADD_METHOD_TO(CourseController::updateVideosOrder, "/courses/{1}/videos/order", Put);
    ADD_METHOD_TO(CourseController::updateVideoInCourse, "/courses/{1}/videos/{2}", Put);
    ADD_METHOD_TO(CourseController::deleteVideo, "/courses/{1}/videos/{2}", Delete);

    // Видео в главах
    ADD_METHOD_TO(CourseController::createVideoInChapter, "/courses/{1}/chapters/{2}/videos", Post);
    ADD_METHOD_TO(CourseController::updateChapterVideosOrder, "/courses/{1}/chapters/{2}/videos/order", Put);
    ADD_METHOD_TO(CourseController::updateVideoInChapter, "/courses/{1}/chapters/{2}/videos/{3}", Put);

    // Перемещение видео
    ADD_METHOD_TO(CourseController::moveVideoPosition, "/courses/{1}/videos/{2}/position", Put);

    // Запись на курс
    ADD_METHOD_TO(CourseController::enrollInCourse, "/courses/{1}/enroll", Post);
    ADD_METHOD_TO(CourseController::unenrollFromCourse, "/courses/{1}/enroll", Delete);
    ADD_METHOD_TO(CourseController::getEnrollments, "/courses/{1}/enrollments", Get);
    ADD_METHOD_TO(CourseController::getEnrolledCourses, "/courses/enrolled", Get);

    // Загрузка файлов
    ADD_METHOD_TO(CourseController::uploadVideoFile, "/courses/{1}/upload", Post);
    METHOD_LIST_END

        // Основные методы курсов
        void getCourses(const HttpRequestPtr& req,
                   std::function<void(const HttpResponsePtr&)>&& callback);
    void createCourse(const HttpRequestPtr& req,
                      std::function<void(const HttpResponsePtr&)>&& callback);
    void getCourseById(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback,
                       const std::string& courseId);
    void updateCourse(const HttpRequestPtr& req,
                      std::function<void(const HttpResponsePtr&)>&& callback,
                      const std::string& courseId);
    void deleteCourse(const HttpRequestPtr& req,
                      std::function<void(const HttpResponsePtr&)>&& callback,
                      const std::string& courseId);

    // Структура курса
    void getCourseStructure(const HttpRequestPtr& req,
                            std::function<void(const HttpResponsePtr&)>&& callback,
                            const std::string& courseId);

    // Управление главами
    void getChapters(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback,
                     const std::string& courseId);
    void createChapter(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback,
                       const std::string& courseId);
    void updateChapter(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback,
                       const std::string& courseId,
                       const std::string& chapterId);
    void deleteChapter(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback,
                       const std::string& courseId,
                       const std::string& chapterId);
    void updateChaptersOrder(const HttpRequestPtr& req,
                             std::function<void(const HttpResponsePtr&)>&& callback,
                             const std::string& courseId);

    // Управление видео (уроками)
    void getVideos(const HttpRequestPtr& req,
                   std::function<void(const HttpResponsePtr&)>&& callback,
                   const std::string& courseId);
    void createVideoInCourse(const HttpRequestPtr& req,
                             std::function<void(const HttpResponsePtr&)>&& callback,
                             const std::string& courseId);
    void createVideoInChapter(const HttpRequestPtr& req,
                              std::function<void(const HttpResponsePtr&)>&& callback,
                              const std::string& courseId,
                              const std::string& chapterId);
    void updateVideoInCourse(const HttpRequestPtr& req,
                             std::function<void(const HttpResponsePtr&)>&& callback,
                             const std::string& courseId,
                             const std::string& videoId);
    void updateVideoInChapter(const HttpRequestPtr& req,
                              std::function<void(const HttpResponsePtr&)>&& callback,
                              const std::string& courseId,
                              const std::string& chapterId,
                              const std::string& videoId);
    void deleteVideo(const HttpRequestPtr& req,
                     std::function<void(const HttpResponsePtr&)>&& callback,
                     const std::string& courseId,
                     const std::string& videoId);
    void updateVideosOrder(const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& callback,
                           const std::string& courseId);
    void updateChapterVideosOrder(const HttpRequestPtr& req,
                                  std::function<void(const HttpResponsePtr&)>&& callback,
                                  const std::string& courseId,
                                  const std::string& chapterId);
    void moveVideoPosition(const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& callback,
                           const std::string& courseId,
                           const std::string& videoId);

    // Запись на курс
    void enrollInCourse(const HttpRequestPtr& req,
                        std::function<void(const HttpResponsePtr&)>&& callback,
                        const std::string& courseId);
    void unenrollFromCourse(const HttpRequestPtr& req,
                            std::function<void(const HttpResponsePtr&)>&& callback,
                            const std::string& courseId);
    void getEnrollments(const HttpRequestPtr& req,
                        std::function<void(const HttpResponsePtr&)>&& callback,
                        const std::string& courseId);
    void getEnrolledCourses(const HttpRequestPtr& req,
                            std::function<void(const HttpResponsePtr&)>&& callback);

    // Загрузка файлов
    void uploadVideoFile(const HttpRequestPtr& req,
                         std::function<void(const HttpResponsePtr&)>&& callback,
                         const std::string& courseId);

private:
    // Структура для информации о файле
    struct FileInfo {
        std::string filename;
        std::string path;
        std::string full_path;
        uint64_t size;
        std::string mime_type;
    };

    // JWT аутентификация
    std::string getCurrentUserId(const HttpRequestPtr& req);
    bool validateJWT(const std::string& token);
    std::string getUserIdFromToken(const std::string& token);
    std::string getRoleFromToken(const std::string& token);
    bool hasPermission(const HttpRequestPtr& req, const std::vector<std::string>& allowedRoles);

    // Валидация
    bool isValidUUID(const std::string& uuid);
    bool isValidCourseData(const Json::Value& data);
    bool isValidChapterData(const Json::Value& data);
    bool isValidVideoData(const Json::Value& data);

    // Вспомогательные методы
    Json::Value createJsonResponse(const std::string& key, const std::string& value);
    Json::Value getCourseJsonResponse(const Courses& course);
    Json::Value getChapterJsonResponse(const CourseChapters& chapter);
    Json::Value getVideoJsonResponse(const CourseVideos& video);

    // Проверка прав доступа
    bool isCourseAuthor(const std::string& userId, const std::string& courseId);
    bool isVideoAuthor(const std::string& userId, const std::string& videoId);
    bool isEnrolledInCourse(const std::string& userId, const std::string& courseId);

    // Методы для работы с файлами
    bool createCourseDirectory(const std::string& courseId);
    bool createChapterDirectory(const std::string& courseId, const std::string& chapterId);
    FileInfo saveVideoFile(const HttpRequestPtr& req, const std::string& courseId, const std::string& chapterId = "");
    FileInfo saveCoverImage(const HttpRequestPtr& req, const std::string& courseId, const std::string& chapterId = "");
    bool deleteFile(const std::string& path);
    std::string getFilePath(const std::string& courseId, const std::string& chapterId,
                            const std::string& filename, bool isCover = false);
    std::string generateFilename(const std::string& originalName);
    bool ensureDirectoryExists(const std::string& path);
    std::string getMimeType(const std::string& filename);

    // Вспомогательный метод для обновления позиции видео
    void updateVideoPosition(const orm::DbClientPtr& dbClient,
                             const std::string& videoId,
                             const std::string& courseId,
                             const std::string& chapterId,
                             int order,
                             const std::function<void(const HttpResponsePtr&)>& callback);

    const std::string JWT_SECRET = "your-super-secret-jwt-key-change-in-production";
    const std::string baseUploadPath_ = "uploads";
};
