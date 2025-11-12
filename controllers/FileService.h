#pragma once

#include <drogon/drogon.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <string>
#include <filesystem>

class FileService {
public:
    static FileService& instance() {
        static FileService instance;
        return instance;
    }

    struct FileInfo {
        std::string filename;
        std::string path;
        std::string full_path;
        uint64_t size;
        std::string mime_type;
    };

    // Создание структуры папок для курса
    bool createCourseDirectory(const std::string& courseId);

    // Создание структуры папок для главы
    bool createChapterDirectory(const std::string& courseId, const std::string& chapterId);

    // Сохранение видео файла
    FileInfo saveVideoFile(const drogon::HttpRequestPtr& req,
                           const std::string& courseId,
                           const std::string& chapterId = "");

    // Сохранение обложки видео
    FileInfo saveCoverImage(const drogon::HttpRequestPtr& req,
                            const std::string& courseId,
                            const std::string& chapterId = "");

    // Удаление файла
    bool deleteFile(const std::string& path);

    // Получение пути для файла
    std::string getFilePath(const std::string& courseId,
                            const std::string& chapterId,
                            const std::string& filename,
                            bool isCover = false);

private:
    FileService() = default;
    std::string baseUploadPath_ = "uploads";

    std::string generateFilename(const std::string& originalName);
    bool ensureDirectoryExists(const std::string& path);
    std::string getMimeType(const std::string& filename);

    // Вспомогательные методы для saveCoverImage
    std::string getCoverSavePath(const std::string& courseId, const std::string& chapterId);
    bool hasValidImageExtension(const std::string& filename);
    void fillFileInfo(FileInfo& fileInfo, const drogon::HttpFile& file,
                      const std::string& fullPath, const std::string& filename);
};
