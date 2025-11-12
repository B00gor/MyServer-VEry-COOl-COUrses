#include "FileService.h"
#include <drogon/drogon.h>
#include <drogon/utils/Utilities.h>
#include <filesystem>
#include <algorithm>
#include <cctype>

using namespace drogon;
namespace fs = std::filesystem;

bool FileService::createCourseDirectory(const std::string& courseId) {
    std::string coursePath = baseUploadPath_ + "/courses/" + courseId;
    return ensureDirectoryExists(coursePath);
}

bool FileService::createChapterDirectory(const std::string& courseId, const std::string& chapterId) {
    std::string chapterPath = baseUploadPath_ + "/courses/" + courseId + "/chapters/" + chapterId;
    return ensureDirectoryExists(chapterPath);
}

FileService::FileInfo FileService::saveVideoFile(const HttpRequestPtr& req,
                                                 const std::string& courseId,
                                                 const std::string& chapterId) {
    FileInfo fileInfo;
    drogon::MultiPartParser fileUpload;

    // Парсим multipart запрос
    if (fileUpload.parse(req) != 0) {
        throw std::runtime_error("Failed to parse multipart request");
    }

    auto& files = fileUpload.getFiles();
    if (files.empty()) {
        throw std::runtime_error("No files found in request");
    }

    auto& file = files[0];

    // Определяем путь для сохранения
    std::string savePath;
    if (chapterId.empty()) {
        savePath = baseUploadPath_ + "/courses/" + courseId + "/videos";
    } else {
        savePath = baseUploadPath_ + "/courses/" + courseId + "/chapters/" + chapterId + "/videos";
    }

    // Создаем директорию если не существует
    if (!ensureDirectoryExists(savePath)) {
        throw std::runtime_error("Failed to create directory: " + savePath);
    }

    // Генерируем уникальное имя файла
    std::string filename = generateFilename(file.getFileName());
    std::string fullPath = savePath + "/" + filename;

    // Сохраняем файл с проверкой успеха операции
    if (!file.saveAs(fullPath)) {
        throw std::runtime_error("Failed to save file: " + fullPath);
    }

    // Заполняем информацию о файле
    fileInfo.filename = filename;
    fileInfo.path = fullPath.substr(baseUploadPath_.length() + 1); // относительный путь
    fileInfo.full_path = fullPath;
    fileInfo.size = file.fileLength(); // Исправлено: fileSize() вместо getFileSize()
    fileInfo.mime_type = getMimeType(filename);

    return fileInfo;
}

FileService::FileInfo FileService::saveCoverImage(const HttpRequestPtr& req,
                                                  const std::string& courseId,
                                                  const std::string& chapterId) {
    FileInfo fileInfo;
    drogon::MultiPartParser fileUpload;

    // Парсим multipart запрос
    if (fileUpload.parse(req) != 0) {
        throw std::runtime_error("Failed to parse multipart request");
    }

    auto& files = fileUpload.getFiles();
    if (files.empty()) {
        throw std::runtime_error("No files found in request");
    }

    bool coverFound = false;
    for (auto& file : files) {
        std::string name = file.getFileName();
        std::string lowerName = name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                       [](unsigned char c){ return std::tolower(c); });

        // Проверяем наличие ключевых слов в имени файла
        if (lowerName.find("cover") != std::string::npos ||
            lowerName.find("image") != std::string::npos ||
            lowerName.find("poster") != std::string::npos) {

            std::string savePath = getCoverSavePath(courseId, chapterId);

            if (!ensureDirectoryExists(savePath)) {
                throw std::runtime_error("Failed to create directory: " + savePath);
            }

            std::string filename = generateFilename(file.getFileName());
            std::string fullPath = savePath + "/" + filename;

            if (!file.saveAs(fullPath)) {
                throw std::runtime_error("Failed to save cover image: " + fullPath);
            }

            fillFileInfo(fileInfo, file, fullPath, filename);
            coverFound = true;
            break;
        }
    }

    // Если не нашли по ключевым словам - ищем по расширению
    if (!coverFound) {
        for (auto& file : files) {
            std::string filename = file.getFileName();
            std::string lowerName = filename;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                           [](unsigned char c){ return std::tolower(c); }); // Исправлено: tolower вместо toLowerCase

            if (hasValidImageExtension(lowerName)) {
                std::string savePath = getCoverSavePath(courseId, chapterId);

                if (!ensureDirectoryExists(savePath)) {
                    throw std::runtime_error("Failed to create directory: " + savePath);
                }

                std::string newFilename = generateFilename(filename);
                std::string fullPath = savePath + "/" + newFilename;

                if (!file.saveAs(fullPath)) {
                    throw std::runtime_error("Failed to save cover image: " + fullPath);
                }

                fillFileInfo(fileInfo, file, fullPath, newFilename);
                coverFound = true;
                break;
            }
        }
    }

    if (!coverFound) {
        throw std::runtime_error("No valid cover image found in request");
    }

    return fileInfo;
}

std::string FileService::getCoverSavePath(const std::string& courseId, const std::string& chapterId) {
    if (chapterId.empty()) {
        return baseUploadPath_ + "/courses/" + courseId + "/covers";
    }
    return baseUploadPath_ + "/courses/" + courseId + "/chapters/" + chapterId + "/covers";
}

bool FileService::hasValidImageExtension(const std::string& filename) {
    std::string lowerName = filename;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    return (lowerName.find(".jpg") != std::string::npos ||
            lowerName.find(".jpeg") != std::string::npos ||
            lowerName.find(".png") != std::string::npos ||
            lowerName.find(".gif") != std::string::npos ||
            lowerName.find(".webp") != std::string::npos);
}

void FileService::fillFileInfo(FileInfo& fileInfo, const drogon::HttpFile& file,
                               const std::string& fullPath, const std::string& filename) {
    fileInfo.filename = filename;
    fileInfo.path = fullPath.substr(baseUploadPath_.length() + 1);
    fileInfo.full_path = fullPath;
    fileInfo.size = file.fileLength(); // Исправлено: fileSize() вместо getFileSize()
    fileInfo.mime_type = getMimeType(filename);
}

bool FileService::deleteFile(const std::string& path) {
    try {
        if (fs::exists(path)) {
            return fs::remove(path);
        }
        return true; // Файл и так не существует
    } catch (const std::exception& e) {
        LOG_ERROR << "Error deleting file: " << e.what();
        return false;
    }
}

std::string FileService::getFilePath(const std::string& courseId,
                                     const std::string& chapterId,
                                     const std::string& filename,
                                     bool isCover) {
    std::string path;
    if (chapterId.empty()) {
        path = baseUploadPath_ + "/courses/" + courseId +
               (isCover ? "/covers/" : "/videos/") + filename;
    } else {
        path = baseUploadPath_ + "/courses/" + courseId + "/chapters/" + chapterId +
               (isCover ? "/covers/" : "/videos/") + filename;
    }
    return path;
}

std::string FileService::generateFilename(const std::string& originalName) {
    // Извлекаем расширение файла
    size_t dotPos = originalName.find_last_of('.');
    std::string extension = (dotPos != std::string::npos) ? originalName.substr(dotPos) : "";

    // Генерируем UUID для имени файла
    std::string uuid = drogon::utils::getUuid();

    return uuid + extension;
}

bool FileService::ensureDirectoryExists(const std::string& path) {
    try {
        return fs::create_directories(path);
    } catch (const std::exception& e) {
        LOG_ERROR << "Error creating directory: " << e.what();
        return false;
    }
}

std::string FileService::getMimeType(const std::string& filename) {
    size_t dotPos = filename.find_last_of('.');
    if (dotPos == std::string::npos) return "application/octet-stream";

    std::string ext = filename.substr(dotPos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    if (ext == "mp4") return "video/mp4";
    if (ext == "avi") return "video/x-msvideo";
    if (ext == "mov") return "video/quicktime";
    if (ext == "webm") return "video/webm";
    if (ext == "mkv") return "video/x-matroska";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "png") return "image/png";
    if (ext == "gif") return "image/gif";
    if (ext == "webp") return "image/webp";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "pdf") return "application/pdf";

    return "application/octet-stream";
}
