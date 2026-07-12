#include "XTFSH/ai/llm_diagnostics.h"
#include "XTFSH/util/io.h"

#include <string>

namespace XTFSH::ai::diag {

const char *http_reason_phrase(int status) {
    switch (status) {
        case 200: return "成功";
        case 400: return "请求无效";
        case 401: return "未授权";
        case 403: return "禁止访问";
        case 404: return "未找到";
        case 408: return "请求超时";
        case 413: return "请求负载过大";
        case 422: return "无法处理的实体";
        case 429: return "请求过于频繁";
        case 500: return "服务器错误";
        case 502: return "网关错误";
        case 503: return "服务不可用";
        case 504: return "网关超时";
        default:  return "HTTP 错误";
    }
}

std::string truncate_for_debug(const std::string &body, std::size_t max_chars) {
    if (body.size() <= max_chars) return body;
    return body.substr(0, max_chars) + "… [已截断]";
}

static void emit_failure(bool final, const std::string &msg) {
    if (final) {
        XTFSH::io::error(msg);
    } else {
        XTFSH::io::warning(msg);
    }
}

void log_http_failure(const std::string &provider,
                      int status,
                      int attempt,
                      int max_attempts,
                      bool final,
                      const std::string &response_body) {
    std::string msg = provider + "：HTTP " + std::to_string(status) + " "
                    + http_reason_phrase(status)
                    + "（第 " + std::to_string(attempt)
                    + "/" + std::to_string(max_attempts) + " 次尝试）";
    if (final) msg += "，已停止重试";
    emit_failure(final, msg);
    if (!response_body.empty()) {
        XTFSH::io::debug(provider + "：响应内容："
                        + truncate_for_debug(response_body));
    }
}

void log_curl_failure(const std::string &provider,
                      const std::string &curl_message,
                      int attempt,
                      int max_attempts,
                      bool final) {
    std::string msg = provider + "：curl 错误：" + curl_message
                    + "（第 " + std::to_string(attempt)
                    + "/" + std::to_string(max_attempts) + " 次尝试）";
    if (final) msg += "，已停止重试";
    emit_failure(final, msg);
}

void log_request_debug(const std::string &provider,
                       const std::string &model,
                       std::size_t body_bytes) {
    if (XTFSH::io::current_log_level() > XTFSH::io::Level::Debug) return;
    XTFSH::io::debug(provider + "：POST " + model
                    + "（请求体 " + std::to_string(body_bytes) + " 字节）");
}

void log_response_debug(const std::string &provider,
                        int status,
                        std::size_t body_bytes,
                        long long elapsed_ms) {
    if (XTFSH::io::current_log_level() > XTFSH::io::Level::Debug) return;
    XTFSH::io::debug(provider + "：HTTP " + std::to_string(status)
                    + "（" + std::to_string(body_bytes) + " 字节，"
                    + std::to_string(elapsed_ms) + " 毫秒）");
}

} // namespace XTFSH::ai::diag
