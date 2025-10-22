//
//  http::RequestHandlers.cpp
//  dungeon_mobile_site
//
//  Created by Vladimir Tolmachev on 15.04.2025.
//

#include "http/Handlers.h"
#include "http/ServerApp.h"
#include "http/utils.h"

namespace http {

FileContent::FileContent(ServerApp &app)
: RequestHandler(app) {
}

Response FileContent::handle(const http::Request &request) {
    try {
        std::string path = "assets/" + std::string(request.get_path());
        std::string body = get_file_content(path);
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

        response.add_header_content_type(get_content_type(request.get_path()));
        response.add_header("ETag", etag);
        response.add_header("Cache-Control", "public, max-age=31536000, immutable");

        return response;
    } catch (...) {
        return Handler404(_app).handle(request);
    }
}

Redirect::Redirect(ServerApp &app, const std::string &redirect)
: RequestHandler(app)
, _redirect(redirect) {
    
}

Response Redirect::handle(const http::Request &request) {
    http::Response response(301, "");
    response.add_header("Location: " + _redirect);
    return response;
}

Handler404::Handler404(ServerApp &app)
: RequestHandler(app) {
};

Response Handler404::handle(const http::Request &request) {
    http::Response response = http::Response404;
    return response;
}

} // namespace http
