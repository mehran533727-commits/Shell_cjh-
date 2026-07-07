#include "CJHSH/util/cwd.h"
#include "CJHSH/util/io.h"

#include <filesystem>
#include <string>
#include <system_error>

namespace CJHSH::util {

std::string current_working_directory() {
    std::error_code ec;
    auto p = std::filesystem::current_path(ec);
    if (ec) {
        CJHSH::io::debug(std::string("current_path failed: ") + ec.message());
        return {};
    }
    return p.string();
}

} // namespace CJHSH::util
