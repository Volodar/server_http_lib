//
//  http::RequestHandlers.cpp
//  dungeon_mobile_site
//
//  Created by Vladimir Tolmachev on 15.04.2025.
//

#include "http/Handlers.h"
#include "http/ServerApp.h"
#include "http/utils.h"
#include "Request.h"

namespace http {

FileContent::FileContent(ServerApp &app, Handler auth_handler)
: RequestHandler(app) {
    set_sequire(auth_handler);
}

Response FileContent::handle(const http::RequestIncoming &request) {
    try {
        std::filesystem::path normalized_path;
        if(!normalize_asset_path(request.get_path(), normalized_path))
            return Response(400, "invalid path");
        
        std::string body = get_file_content(normalized_path.string());
        if (body.empty())
            return Handler404(_app).handle(request);
    
        const std::string etag = "\"" + sha256(body) + "\"";

        http::Response response;
        auto inm = request.get_headers().get("If-None-Match");
        if (!inm.empty() && inm == etag) {
            response.code = 304;
        } else {
            response.code = 200;
            response.body = std::move(body);
        }
        auto content_type = get_content_type(request.get_path());
        response.add_header_content_type(content_type);
        if(content_type != ContentType::Html){
            response.add_header("ETag", etag);
            response.add_header("Cache-Control", "public, max-age=31536000, immutable");
        }

        return response;
    } catch (...) {
        return Handler404(_app).handle(request);
    }
}

bool FileContent::normalize_asset_path(std::string_view input, std::filesystem::path &out_path){
    std::string clean(input);
    int iterations = 3;
    while(clean.find('%') && iterations-- > 0){
        clean = url_decode(clean);
    }
    
    for(char c : clean){
        unsigned char uc = static_cast<unsigned char>(c);
        if(uc < 32 || uc == 127 || uc == '%')
            return false;
    }
    replace(clean, '\\', '/');
    if(!clean.empty() && clean[0] == '/')
        clean.erase(0, 1);

    std::filesystem::path rel(clean);
    if(rel.is_absolute() || rel.has_root_path())
        return false;
    if(!rel.empty() && *rel.begin() == "assets"){
        std::filesystem::path stripped;
        auto it = rel.begin();
        ++it;
        for(; it != rel.end(); ++it)
            stripped /= *it;
        rel = stripped;
    }
    rel = rel.lexically_normal();
    if(rel.empty() || rel == ".")
        return false;
    for(const auto &part : rel){
        if(part == "..")
            return false;
    }
    out_path = std::filesystem::path("assets") / rel;
    return true;
}

Redirect::Redirect(ServerApp &app, const std::string& redirect)
: RequestHandler(app)
, _redirect(redirect) {
    
}

Response Redirect::handle(const http::RequestIncoming &request) {
    http::Response response(301, "");
    response.add_header("Location", _redirect);
    return response;
}

Response Handler404::handle(const http::RequestIncoming &request) {
    http::Response response = http::Response404;
    return response;
}

Response HealthHandler::handle(const http::RequestIncoming &) {
#if SERVER_WEB_HAVE_MYSQLCONNECTOR == 1
    auto res = _app.get_mysql()->query_get("SELECT DATABASE(), 1");
    http::Response r(200, (res && res->next()) ? "{\"status\":\"ok\"}" : "{\"status\":\"sql fail\"}");
#else
    http::Response r(200, "{\"status\":\"ok\"}");
#endif
    r.add_header_content_type(ContentType::Json);
    return r;
}

} // namespace http
