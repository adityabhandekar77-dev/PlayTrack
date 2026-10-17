
#include "app_discovery.h"

#include <algorithm>
#include <cwctype>
#include <string>

static bool isExecutable(const std::filesystem::path& path) {
    return path.has_extension() &&
           _wcsicmp(path.extension().c_str(), L".exe") == 0;
}

static std::wstring normalizePath(const std::filesystem::path& path) {
    std::error_code error;
    auto absolutePath = std::filesystem::weakly_canonical(path, error);

    if (error) {
        error.clear();
        absolutePath = std::filesystem::absolute(path, error);
        if (error) absolutePath = path;
    }

    return absolutePath.lexically_normal().wstring();
}

static bool samePath(
    const std::filesystem::path& first,
    const std::filesystem::path& second
) {
    return _wcsicmp(
        normalizePath(first).c_str(),
        normalizePath(second).c_str()
    ) == 0;
}

std::vector<Application> discoverApplications(
    const std::vector<std::filesystem::path>& folders
) {
    std::vector<Application> discovered;

    for (const auto& folder : folders) {
        std::error_code error;

        if (!std::filesystem::is_directory(folder, error)) {
            continue;
        }

        std::filesystem::recursive_directory_iterator iterator(
            folder,
            std::filesystem::directory_options::skip_permission_denied,
            error
        );

        std::filesystem::recursive_directory_iterator end;

        while (iterator != end) {
            if (error) {
                error.clear();
                iterator.increment(error);
                continue;
            }

            const auto& entry = *iterator;
            std::error_code entryError;

            if (entry.is_symlink(entryError) && entry.is_directory(entryError)) {
                iterator.disable_recursion_pending();
            } else if (entry.is_regular_file(entryError) &&
                       isExecutable(entry.path())) {
                bool duplicate = false;

                for (const auto& app : discovered) {
                    if (samePath(app.executable, entry.path())) {
                        duplicate = true;
                        break;
                    }
                }

                if (!duplicate) {
                    Application app;
                    app.name = entry.path().stem().string();
                    app.executable = normalizePath(entry.path());
                    discovered.push_back(std::move(app));
                }
            }

            iterator.increment(error);
        }
    }

    return discovered;
}

void mergeDiscoveredApplications(
    std::vector<Application>& existing,
    const std::vector<Application>& discovered
) {
    for (const auto& candidate : discovered) {
        bool duplicate = false;

        for (const auto& app : existing) {
            if (samePath(app.executable, candidate.executable)) {
                duplicate = true;
                break;
            }
        }

        if (!duplicate) {
            existing.push_back(candidate);
        }
    }
}
