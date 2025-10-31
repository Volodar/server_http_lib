//
//  Renderer.cpp
//  dungeon2_server_analytics
//
//  Created by Vladimir Tolmachev on 14.04.2025.
//

#include "Renderer.h"
#include "http/utils.h"
#include <unordered_map>

namespace http {
Renderer::Renderer(ServerApp &app) {
    _head = "<html><head><title>Title</title></head><body>";
    _menu = "<nav>Menu</nav>";
    _footer = "</body></html>";
}

std::string Renderer::get_template() { return ""; }

void Renderer::add_head(std::string& page) { page += _head; }
void Renderer::add_menu(std::string& page) { page += _menu; }
void Renderer::add_footer(std::string& page) { page += _footer; }
void Renderer::add_body(std::string& page, const std::string& content) {
    page += content;
}
void Renderer::set_variables(
    const std::unordered_map<std::string, std::string> &vars) {
    _vars = vars;
}
void Renderer::add_variables(
    const std::unordered_map<std::string, std::string> &vars) {
    _vars.insert(vars.begin(), vars.end());
}
std::string Renderer::render_template(const std::string& tmplt) {
    return http::format(tmplt, _vars);
}
} // namespace http
