#include "test_framework.h"
#include "http/Scheduler.h"
#include "asio.hpp"

#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

using namespace std::chrono;

TEST(Scheduler_League_Period_OneWeek)
{
    asio::io_context io;
    http::Scheduler s(io);
    ASSERT_EQ(s.get_league_period().count(), 7 * 24 * 3600);
}

// Вспомогательная функция: создать time_point из локального tm
static std::chrono::system_clock::time_point make_tp_local(std::tm tm)
{
    std::time_t tt = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(tt);
}

TEST(Scheduler_TimePointToString_LocalFormatting)
{
    std::tm tm{};
    tm.tm_year = 123; // 2023
    tm.tm_mon = 0;    // Jan
    tm.tm_mday = 2;   // 02
    tm.tm_hour = 3;
    tm.tm_min = 4;
    tm.tm_sec = 5;

    auto tp = make_tp_local(tm);
    auto s = http::Scheduler::time_point_to_string(tp);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y/%m/%d %H:%M:%S");
    ASSERT_EQ(s, oss.str());
}

TEST(Scheduler_Timestamp_Formatters)
{
    std::tm tm{};
    tm.tm_year = 123; // 2023
    tm.tm_mon = 6;    // July
    tm.tm_mday = 8;   // 08
    tm.tm_hour = 9;
    tm.tm_min = 10;
    tm.tm_sec = 11;

    std::time_t tt = std::mktime(&tm);

    std::ostringstream date_oss;
    date_oss << std::put_time(&tm, "%Y/%m/%d");
    std::ostringstream time_oss;
    time_oss << std::put_time(&tm, "%H:%M:%S");

    ASSERT_EQ(http::timestamp_to_date(tt), date_oss.str());
    ASSERT_EQ(http::timestamp_to_time(tt), time_oss.str());
}

TEST(Scheduler_NextDailyPoint_FutureTodayOrTomorrow)
{
    asio::io_context io;
    http::Scheduler s(io);

    // Выбираем цель через ~2 минуты от текущего времени
    auto now = system_clock::now();
    auto future = now + minutes(2);
    std::time_t future_tt = system_clock::to_time_t(future);
    std::tm future_tm = *std::localtime(&future_tt);

    auto tp = s.next_daily_point(future_tm.tm_hour, future_tm.tm_min, future_tm.tm_sec);

    // Должно совпасть (с небольшим допуском) с выбранным future
    auto diff = duration_cast<seconds>(tp - make_tp_local(future_tm));
    ASSERT_TRUE(diff <= seconds(1) && diff >= seconds(-1));

    // И быть строго в будущем относительно now
    ASSERT_TRUE(tp > now);
}

TEST(Scheduler_NextWeeklyPoint_FutureWithinWeek)
{
    asio::io_context io;
    http::Scheduler s(io);

    // Выбираем целевой день недели через 3 дня от now и фиксированное время суток
    auto now = system_clock::now();
    auto in3d = now + hours(72);
    std::time_t tt = system_clock::to_time_t(in3d);
    std::tm t = *std::localtime(&tt);

    int target_wday = t.tm_wday;
    int th = 9, tmn = 30, ts = 15;

    auto tp = s.next_weekly_point(target_wday, th, tmn, ts);
    std::time_t tp_tt = system_clock::to_time_t(tp);
    std::tm tp_tm = *std::localtime(&tp_tt);

    ASSERT_EQ(tp_tm.tm_wday, target_wday);
    ASSERT_EQ(tp_tm.tm_hour, th);
    ASSERT_EQ(tp_tm.tm_min, tmn);
    ASSERT_EQ(tp_tm.tm_sec, ts);
    ASSERT_TRUE(tp > now);
    ASSERT_TRUE(tp - now <= hours(24 * 7));
}

TEST(Scheduler_NextWeeklyPoint_PastToday_GoesNextWeek)
{
    asio::io_context io;
    http::Scheduler s(io);

    auto now = system_clock::now();
    std::time_t now_tt = system_clock::to_time_t(now);
    std::tm now_tm = *std::localtime(&now_tt);

    int target_wday = now_tm.tm_wday;
    // Выбираем целевое время суток, которое гарантированно не позже текущего
    // (<= now), чтобы логика перевела событие на следующую неделю.
    int th = now_tm.tm_hour;
    int tmn = now_tm.tm_min;
    int ts = now_tm.tm_sec > 0 ? now_tm.tm_sec - 1 : 0; // не позже текущего

    auto tp = s.next_weekly_point(target_wday, th, tmn, ts);
    std::time_t tp_tt = system_clock::to_time_t(tp);
    std::tm tp_tm = *std::localtime(&tp_tt);

    ASSERT_EQ(tp_tm.tm_wday, target_wday);
    ASSERT_EQ(tp_tm.tm_hour, th);
    ASSERT_EQ(tp_tm.tm_min, tmn);
    ASSERT_EQ(tp_tm.tm_sec, ts);
    ASSERT_TRUE(tp > now);
    // При выборе времени не позже текущего перенос обязателен на следующую неделю.
    ASSERT_TRUE(tp - now > hours(24));
}

TEST(Scheduler_ScheduleAfter_ExecutesJob)
{
    asio::io_context io;
    http::Scheduler s(io);
    std::atomic<int> hits{0};

    s.schedule_after(std::chrono::milliseconds(20), [&] { ++hits; });
    io.run();
    ASSERT_EQ(hits.load(), 1);
}

TEST(Scheduler_ScheduleAt_ExecutesJob)
{
    asio::io_context io;
    http::Scheduler s(io);
    std::atomic<int> hits{0};

    auto ts = system_clock::now() + milliseconds(30);
    s.schedule_at(ts, [&] { ++hits; });
    io.run();
    ASSERT_EQ(hits.load(), 1);
}
