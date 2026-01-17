/*
 * Copyright 2014-2015 Vladimir Tolmachev
 *
 * Author: Vladimir Tolmachev
 * e-mail: tolm_vl@hotmail.com
 * If you received the code is not the author, please contact me
 */

#ifndef __http_utils_h__
#define __http_utils_h__

#include "jsoncpp/json.h"
#include <cassert>
#include <string>
#include <unordered_map>

namespace http {

// Common Content-Type constants used by get_content_type
namespace ContentType {
inline constexpr const char *SevenZip = "application/x-7z-compressed";
inline constexpr const char *Bmp = "image/bmp";
inline constexpr const char *Css = "text/css";
inline constexpr const char *Csv = "text/csv";
inline constexpr const char *Doc = "application/msword";
inline constexpr const char *Docx =
    "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
inline constexpr const char *Eot = "application/vnd.ms-fontobject";
inline constexpr const char *Gif = "image/gif";
inline constexpr const char *Gzip = "application/gzip";
inline constexpr const char *Htm = "text/html";
inline constexpr const char *Html = "text/html";
inline constexpr const char *Ico = "image/x-icon";
inline constexpr const char *Jpeg = "image/jpeg";
inline constexpr const char *Jpg = "image/jpeg";
inline constexpr const char *Js = "application/javascript";
inline constexpr const char *Json = "application/json";
inline constexpr const char *Mp3 = "audio/mpeg";
inline constexpr const char *Mp4 = "video/mp4";
inline constexpr const char *Ogg = "audio/ogg";
inline constexpr const char *Ogv = "video/ogg";
inline constexpr const char *Pdf = "application/pdf";
inline constexpr const char *Png = "image/png";
inline constexpr const char *Ppt = "application/vnd.ms-powerpoint";
inline constexpr const char *Pptx =
    "application/vnd.openxmlformats-officedocument.presentationml.presentation";
inline constexpr const char *Rar = "application/vnd.rar";
inline constexpr const char *Svg = "image/svg+xml";
inline constexpr const char *Tar = "application/x-tar";
inline constexpr const char *Ttf = "font/ttf";
inline constexpr const char *Txt = "text/plain";
inline constexpr const char *Wav = "audio/wav";
inline constexpr const char *Webm = "video/webm";
inline constexpr const char *Woff = "font/woff";
inline constexpr const char *Woff2 = "font/woff2";
inline constexpr const char *Wasm = "application/wasm";
inline constexpr const char *Xls = "application/vnd.ms-excel";
inline constexpr const char *Xlsx =
    "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
inline constexpr const char *Xml = "application/xml";
inline constexpr const char *Zip = "application/zip";
inline constexpr const char *OctetStream =
    "application/octet-stream"; // default fallback
} // namespace ContentType

std::string get_file_content(const std::string_view &path);
std::string_view get_file_ext(const std::string_view &path);
std::string get_content_type(const std::string_view &path);

Json::Value read_json_from_string(const std::string& string);

void replace(std::string& str, const std::string& from, const std::string& to);
void replace(std::string& str, char from, char to);

template <typename T> std::string to_string(const T &value);

std::string format(const std::string& template_str,
                   const std::unordered_map<std::string, std::string> &values);

template <typename... Args>
std::string format_indexes(const std::string& template_str, Args &&...args) {
    std::string query = template_str;
    std::tuple<Args...> values(std::forward<Args>(args)...);

    auto replace = [&](int index, const std::string& value) {
        std::string token = "$" + std::to_string(index);
        size_t pos = 0;
        bool was_replace = false;
        while ((pos = query.find(token, pos)) != std::string::npos) {
            query.replace(pos, token.length(), value);
            pos += value.length();
            was_replace = true;
        }
        (void)was_replace;
        assert(was_replace);
    };

    std::apply(
        [&](auto &&...unpacked) {
            int i = 0;
            ((replace(i++, to_string(unpacked))), ...);
        },
        values);

    assert(query.find("$") == std::string::npos);
    return query;
}

int hex_val(char ch);
std::string url_encode(const std::string_view& input);
std::string url_decode(const std::string_view& input);
std::string sha256(const std::string& input);

template <typename T>
std::string join(const std::vector<T> &values, const char delimiter = ',') {
    std::string result;
    int index = 0;
    size_t size = values.size();
    for (auto &t : values) {
        result += std::to_string(t);
        if (index < (size - 1))
            result.push_back(delimiter);
        ++index;
    }
    return result;
}

std::vector<std::string> split(const std::string& values, char delimiter);
void strip(std::string& value);

void sv_lstrip(std::string_view &sv);
void sv_rstrip(std::string_view &sv);
void sv_strip(std::string_view &sv);
std::vector<std::string_view> sv_split(const std::string_view& values, char delimiter);

} // namespace http
#endif
