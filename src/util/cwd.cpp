#include "XTFSH/util/cwd.h"
#include "XTFSH/util/io.h"

#include <filesystem>
#include <string>
#include <system_error>

namespace XTFSH::util {

std::string current_working_directory() {
    std::error_code ec;
    auto p = std::filesystem::current_path(ec);
    if (ec) {
        XTFSH::io::debug(std::string("获取当前路径失败：") + ec.message());
        return {};
    }
    return p.string();
}

} // namespace XTFSH::util
