#include "pch.hpp"
#include "logger.hpp"

#include <fstream>
#include <mutex>
#include <filesystem>
#include <chrono>

namespace
{
    std::mutex g_log_mutex;
    std::string g_open_day{};
    std::ofstream g_log;

    std::string today_ymd()
    {
        const auto now = std::chrono::system_clock::now();
        const std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        char buf[16]{};
        std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
        return buf;
    }

    std::string now_iso()
    {
        const auto now = std::chrono::system_clock::now();
        const std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        char buf[32]{};
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
        return buf;
    }

    void ensure_log_open()
    {
        const std::string day = today_ymd();
        if (g_log.is_open() && g_open_day == day)
            return;

        if (g_log.is_open())
            g_log.close();

        std::error_code ec;
        std::filesystem::create_directories("logs", ec);

        const std::string path = std::format("logs/gurotopia-{}.log", day);
        g_log.open(path, std::ios::out | std::ios::app);
        if (g_log)
        {
            g_log.rdbuf()->pubsetbuf(nullptr, 0); // @note unbuffered / line-ish flush via endl
            g_open_day = day;
        }
        else
            g_open_day.clear();
    }
}

void log_event(
    const std::string &category,
    const std::string &who,
    const std::string &world,
    const std::string &detail)
{
    std::lock_guard lock(g_log_mutex);
    ensure_log_open();
    if (!g_log)
        return;

    g_log << now_iso() << " | " << category << " | " << who
          << " | " << (world.empty() ? "-" : world)
          << " | " << detail << '\n';
    g_log.flush();
}
