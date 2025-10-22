//
//  Scheduler.hpp
//  server_web
//
//  Created by Vladimir Tolmachev on 24.03.2025.
//

#ifndef Scheduler_hpp
#define Scheduler_hpp

#include <asio.hpp>
#include <chrono>
#include <ctime>
#include <functional>
#include <memory>

namespace http {

class Scheduler {
  public:
    explicit Scheduler(asio::io_context &io);

    std::chrono::seconds get_league_period() const;
    std::chrono::system_clock::time_point get_league_start_time() const;
    std::chrono::system_clock::time_point get_league_finish_time() const;

    int get_time_int() const;
    static std::chrono::system_clock::time_point get_time();
    std::chrono::system_clock::time_point current_week_point() const;
    std::chrono::system_clock::time_point next_week_point() const;
    std::chrono::system_clock::time_point
    next_daily_point(int hour = 0, int munutes = 0, int seconds = 0) const;
    std::chrono::system_clock::time_point
    next_weekly_point(int target_wday, int hour = 0, int munutes = 0,
                      int seconds = 0) const;
    static std::string
    time_point_to_string(const std::chrono::system_clock::time_point &tp);

    void schedule_after(std::chrono::system_clock::duration delay,
                        std::function<void()> job);
    void schedule_at(std::chrono::system_clock::time_point timestamp,
                     std::function<void()> job);

  private:
    asio::io_context &_io;
};

std::string timestamp_to_date(time_t time);
std::string timestamp_to_time(time_t time);

} // namespace http
#endif /* Scheduler_hpp */
