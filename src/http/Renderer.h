//
//  Renderer.hpp
//  dungeon2_server_analytics
//
//  Created by Vladimir Tolmachev on 14.04.2025.
//

#ifndef Renderer_hpp
#define Renderer_hpp

#include <string>
#include <unordered_map>

namespace http {
class ServerApp;

class Renderer {
  public:
    Renderer(ServerApp &app);

    std::string get_template();
    void
    set_variables(const std::unordered_map<std::string, std::string> &vars);
    void
    add_variables(const std::unordered_map<std::string, std::string> &vars);
    std::string render_template(const std::string& tmplt);

    void set_head(const std::string& tmplt) { _head = tmplt; }
    void set_menu(const std::string& tmplt) { _menu = tmplt; }
    void set_footer(const std::string& tmplt) { _footer = tmplt; }

    void add_head(std::string& page);
    void add_menu(std::string& page);
    void add_footer(std::string& page);
    void add_body(std::string& page, const std::string& content);

  private:
    std::string _head;
    std::string _menu;
    std::string _footer;
    std::unordered_map<std::string, std::string> _vars;
};

} // namespace http
#endif /* Renderer_hpp */
