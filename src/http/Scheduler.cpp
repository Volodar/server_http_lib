//
//  Scheduler.cpp
//  server_web
//
//  Created by Vladimir Tolmachev on 24.03.2025.
//

#include "Scheduler.h"
#include <iomanip>

namespace http {

Scheduler::Scheduler(asio::io_context &io) : _io(io) {}

std::chrono::seconds Scheduler::get_league_period() const {
    auto period = 3600 * 24 * 7; // 1 week
    return std::chrono::seconds(period);
}

std::chrono::system_clock::time_point Scheduler::get_league_start_time() const {
    return current_week_point();
}
std::chrono::system_clock::time_point
Scheduler::get_league_finish_time() const {
    return next_week_point();
}

int Scheduler::get_time_int() const {
    return static_cast<int>(std::chrono::system_clock::to_time_t(get_time()));
}
std::chrono::system_clock::time_point Scheduler::get_time() {
    return std::chrono::system_clock::now();
}

std::chrono::system_clock::time_point Scheduler::current_week_point() const {
    return this->next_weekly_point(-6);
}

std::chrono::system_clock::time_point Scheduler::next_week_point() const {
    return this->next_weekly_point(1);
}

std::chrono::system_clock::time_point
Scheduler::next_daily_point(int hour, int munutes, int seconds) const {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto now_time_t = system_clock::to_time_t(now);
    std::tm now_tm = *std::localtime(&now_time_t);

    now_tm.tm_hour = hour;
    now_tm.tm_min = munutes;
    now_tm.tm_sec = seconds;

    auto desired_time = system_clock::from_time_t(std::mktime(&now_tm));

    if (desired_time <= now) {
        // Если заданное время уже прошло сегодня, переходим на следующий день
        desired_time += hours(24);
    }

    return desired_time;
}

std::chrono::system_clock::time_point
Scheduler::next_weekly_point(int target_wday, int hour, int munutes,
                             int seconds) const {
    auto time_of_day = std::chrono::hours(hour) +
                       std::chrono::minutes(munutes) +
                       std::chrono::seconds(seconds);

    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm = *std::localtime(&tt);

    // tm_wday: 0 = Sunday, 1 = Monday, ..., 6 = Saturday
    int current_wday = local_tm.tm_wday;
    int days_to_add = (target_wday - current_wday + 7) % 7;
    if (days_to_add == 0) {
        // Если уже сегодня, проверим, прошло ли указанное время
        auto now_day_seconds = std::chrono::hours(local_tm.tm_hour) +
                               std::chrono::minutes(local_tm.tm_min) +
                               std::chrono::seconds(local_tm.tm_sec);
        if (now_day_seconds >= time_of_day) {
            days_to_add = 7; // переносим на следующую неделю
        }
    }

    local_tm.tm_hour = 0;
    local_tm.tm_min = 0;
    local_tm.tm_sec = 0;
    local_tm.tm_mday += days_to_add;

    std::time_t base_day_tt = std::mktime(&local_tm);
    auto base_day_tp = std::chrono::system_clock::from_time_t(base_day_tt);

    return base_day_tp + time_of_day;
}

std::string Scheduler::time_point_to_string(
    const std::chrono::system_clock::time_point &tp) {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::localtime(&t);

    std::ostringstream oss;

    oss << std::put_time(&tm, "%Y/%m/%d %H:%M:%S");
    std::string result = oss.str();
    return result;
}

void Scheduler::schedule_after(std::chrono::system_clock::duration delay,
                               std::function<void()> job) {
    auto timer = std::make_shared<asio::steady_timer>(_io, delay);
    timer->async_wait(
        [job = std::move(job), timer](const asio::error_code &ec) {
            if (!ec)
                job();
        });
}

void Scheduler::schedule_at(std::chrono::system_clock::time_point timestamp,
                            std::function<void()> job) {
    auto now = std::chrono::system_clock::now();
    auto delay = (timestamp > now) ? timestamp - now : std::chrono::seconds(0);
    schedule_after(delay, std::move(job));
}

std::string timestamp_to_date(time_t time) {
    std::tm tm = *std::localtime(&time);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y/%m/%d");
    return oss.str();
}
std::string timestamp_to_time(time_t time) {
    std::tm tm = *std::localtime(&time);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S");
    return oss.str();
}

} // namespace http
