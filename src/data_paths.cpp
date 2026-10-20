#include "data_paths.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>

#include <system_error>

std::filesystem::path getDataDirectory() {
    PWSTR localAppData = nullptr;

    HRESULT result = SHGetKnownFolderPath(
        FOLDERID_LocalAppData, 0, nullptr, &localAppData
    );

    if (FAILED(result) || !localAppData) {
        return {};
    }

    std::filesystem::path directory =
        std::filesystem::path(localAppData) / L"PlayTrack";

    CoTaskMemFree(localAppData);
    return directory;
}

bool initializeDataDirectory(
    const std::filesystem::path& appDirectory
) {
    const auto dataDirectory = getDataDirectory();
    if (dataDirectory.empty()) return false;

    std::error_code error;
    std::filesystem::create_directories(dataDirectory, error);
    if (error) return false;

    for (const auto* name : {"config.json", "playtime.json"}) {
        const auto source = appDirectory / name;
        const auto destination = dataDirectory / name;

        if (!std::filesystem::exists(destination, error) &&
            !error &&
            std::filesystem::is_regular_file(source, error) &&
            !error) {
            std::filesystem::copy_file(
                source, destination,
                std::filesystem::copy_options::none, error
            );
            error.clear();
        }

        error.clear();
    }

    const auto sourceFolder = appDirectory / L"tracked-apps";
    const auto destinationFolder = dataDirectory / L"tracked-apps";

    if (!std::filesystem::exists(destinationFolder, error) &&
        !error &&
        std::filesystem::is_directory(sourceFolder, error) &&
        !error) {
        std::filesystem::copy(
            sourceFolder, destinationFolder,
            std::filesystem::copy_options::recursive, error
        );
    }

    error.clear();
    std::filesystem::create_directories(destinationFolder, error);

    return !error;
}
