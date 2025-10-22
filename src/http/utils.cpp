/*
 * Copyright 2014-2015 Vladimir Tolmachev
 *
 * Author: Vladimir Tolmachev
 * e-mail: tolm_vl@hotmail.com
 * If you received the code is not the author, please contact me
 */

#include "utils.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <openssl/sha.h>
#include <sstream>

namespace http {

std::string get_file_content(const std::string_view &path) {
    std::ifstream file(path.data());
    if (!file.is_open()) {
        return "";
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string_view get_file_ext(const std::string_view &path) {
    return path.substr(path.find_last_of('.'));
}

std::string get_content_type(const std::string_view &path) {
    static const std::unordered_map<std::string_view, std::string> types = {
        {".7z", ContentType::SevenZip}, {".bmp", ContentType::Bmp},
        {".css", ContentType::Css},     {".csv", ContentType::Csv},
        {".doc", ContentType::Doc},     {".docx", ContentType::Docx},
        {".eot", ContentType::Eot},     {".gif", ContentType::Gif},
        {".gz", ContentType::Gzip},     {".htm", ContentType::Htm},
        {".html", ContentType::Html},   {".ico", ContentType::Ico},
        {".jpeg", ContentType::Jpeg},   {".jpg", ContentType::Jpg},
        {".js", ContentType::Js},       {".json", ContentType::Json},
        {".mp3", ContentType::Mp3},     {".mp4", ContentType::Mp4},
        {".ogg", ContentType::Ogg},     {".ogv", ContentType::Ogv},
        {".pdf", ContentType::Pdf},     {".png", ContentType::Png},
        {".ppt", ContentType::Ppt},     {".pptx", ContentType::Pptx},
        {".rar", ContentType::Rar},     {".svg", ContentType::Svg},
        {".tar", ContentType::Tar},     {".ttf", ContentType::Ttf},
        {".txt", ContentType::Txt},     {".wav", ContentType::Wav},
        {".webm", ContentType::Webm},   {".woff", ContentType::Woff},
        {".woff2", ContentType::Woff2}, {".wasm", ContentType::Wasm},
        {".xls", ContentType::Xls},     {".xlsx", ContentType::Xlsx},
        {".xml", ContentType::Xml},     {".zip", ContentType::Zip},
    };
    auto ext = get_file_ext(path);
    auto it = types.find(ext);
    return it != types.end() ? it->second : ContentType::OctetStream;
}

Json::Value read_json_from_string(const std::string& string)
{
    Json::Value json;
    Json::Reader reader;
    reader.parse(string, json);
    return json;
}

void replace(std::string &str, const std::string &from, const std::string &to) {
    if (from.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

template <> std::string to_string(const int &value) {
    return std::to_string(value);
}
template <> std::string to_string(const unsigned int &value) {
    return std::to_string(value);
}
template <> std::string to_string(const long &value) {
    return std::to_string(value);
}
template <> std::string to_string(const long long &value) {
    return std::to_string(value);
}
template <> std::string to_string(const float &value) {
    return std::to_string(value);
}
template <> std::string to_string(const std::string &value) { return value; }

std::string format(const std::string &template_str, const std::unordered_map<std::string, std::string> &values) {
    std::string result = template_str;
    for (auto &&[key, value] : values) {
        replace(result, "$" + key + "$", to_string(value));
    }
    return result;
}

static inline int hex_val(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
    if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
    return -1;
}

std::string url_decode(const std::string &input) {
    std::string out;
    out.reserve(input.size());
    size_t i = 0;
    while (i < input.size()) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        if (c == '%') {
            if (i + 2 < input.size()) {
                int hi = hex_val(input[i + 1]);
                int lo = hex_val(input[i + 2]);
                if (hi >= 0 && lo >= 0) {
                    out.push_back(static_cast<char>((hi << 4) | lo));
                    i += 3;
                    continue;
                }
            }
            out.push_back('%');
            ++i;
        } else if (c == '+') {
            out.push_back(' ');
            ++i;
        } else {
            out.push_back(static_cast<char>(c));
            ++i;
        }
    }
    return out;
}
std::string url_encode(const std::string &input) {
    std::string out;
    out.reserve(input.size() * 3);
    auto hex_upper = [](unsigned char v) -> char {
        static const char* HEX = "0123456789ABCDEF";
        return HEX[v & 0x0F];
    };
    for (unsigned char c : input) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else if (c == ' ') {
            out.push_back('+');
        } else {
            out.push_back('%');
            out.push_back(hex_upper((c >> 4) & 0x0F));
            out.push_back(hex_upper(c & 0x0F));
        }
    }
    return out;
}

std::string sha256(const std::string &input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char *>(input.c_str()), input.length(), hash);

    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return ss.str();
}

std::vector<std::string> split(const std::string &values, char delimiter) {
    std::vector<std::string> out;
    if (!values.empty()) {
        std::string string = values;
        do {
            size_t k = string.find_first_of(delimiter);
            if (k == -1) {
                out.push_back(string);
                break;
            }
            out.push_back(string.substr(0, k));
            string = string.substr(k + 1);
            if (string.empty())
                break;
        } while (true);
    }
    return out;
}

void strip(std::string &value) {
    auto start = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) { return std::isspace(ch); });
    value = std::string(start, value.end());
    
    auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
    value = std::string(value.begin(), end);
}

void sv_lstrip(std::string_view &sv) {
    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t'))
        sv.remove_prefix(1);
}
void sv_rstrip(std::string_view &sv) {
    while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t'))
        sv.remove_suffix(1);
}
void sv_strip(std::string_view &sv) {
    sv_lstrip(sv);
    sv_rstrip(sv);
}

} // namespace http
