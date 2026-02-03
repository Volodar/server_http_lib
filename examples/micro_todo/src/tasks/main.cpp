#include "http/ServerApp.h"
#include "http/http_common.h"
#include "src/common/ports.h"

using namespace http;


class GetTasks : public RequestHandler {
  public:
    explicit GetTasks(ServerApp &app) : RequestHandler(app) {}
    Response handle(const RequestIncoming &) override {
      Response r(200, R"({"tasks":[{"id":1,"title":"Buy milk"},{"id":2,"title":"Write code"}]})");
      r.add_header_content_type("application/json");
      return r;
    }
};

class CreateTask : public RequestHandler {
  public:
    explicit CreateTask(ServerApp &app) : RequestHandler(app) {}
    Response handle(const RequestIncoming &req) override {
      std::string posted(req.get_data());
      std::string body = std::string(R"({"created":true,"data":)") + (posted.empty() ? "{}" : posted) + "}";
      Response r(200, std::move(body));
      r.add_header_content_type("application/json");
      return r;
    }
};


int main() {
  http::ServerApp app(micro_todo::PORT_TASKS);

  app.add_endpoint<http::HealthHandler>("/health", http::Method::get);
  app.add_endpoint<GetTasks>("/tasks", http::Method::get);
  app.add_endpoint<CreateTask>("/tasks", http::Method::post);

  app.run(1);
  return 0;
}
