#include "http/ServerApp.h"
#include "http/http_common.h"
#include "src/common/ports.h"

using namespace http;


class GetUsers : public RequestHandler {
  public:
    explicit GetUsers(ServerApp &app) : RequestHandler(app) {}
    Response handle(const RequestIncoming &) override {
      Response r(200, R"({"users":[{"id":1,"name":"Alice"},{"id":2,"name":"Bob"}]})");
      r.add_header_content_type("application/json");
      return r;
    }
};

class CreateUser : public RequestHandler {
  public:
    explicit CreateUser(ServerApp &app) : RequestHandler(app) {}
    Response handle(const RequestIncoming &req) override {
      std::string posted = std::string(req.get_data());
      std::string body = std::string(R"({"created":true,"data":)") + (posted.empty() ? "{}" : posted) + "}";
      Response r(200, std::move(body));
      r.add_header_content_type("application/json");
      return r;
    }
};

int main() {
  http::ServerApp app(micro_todo::PORT_USERS);

  app.add_endpoint<http::HealthHandler>("/health", http::Method::get);
  app.add_endpoint<GetUsers>("/users", http::Method::get);
  app.add_endpoint<CreateUser>("/users", http::Method::post);

  app.run(1);
  return 0;
}
