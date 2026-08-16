#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <initializer_list>
#include <iostream>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>


namespace m0rtis::log {

    enum class Level { Debug, Info, Warn, Error };

    // one key=value pair tacked onto a log line - build these with field()
    // below rather than constructing directly, it takes care of the
    // stringification.
    struct Field {
        std::string key;
        std::string value;
    };

    template <typename T>
    inline Field field(std::string_view key, const T &value) {
        std::ostringstream oss;
        oss << value;
        return Field{std::string(key), oss.str()};
    }

    // string_view overload so field("addr", ip_str) doesn't go through
    // ostringstream for the common case of "it's already a string"
    inline Field field(std::string_view key, std::string_view value) {
        return Field{std::string(key), std::string(value)};
    }

    // uint8_t is unsigned char, so without this overload the generic
    // template above would stream vnode_id as a raw (often unprintable)
    // character instead of a number - vnode_id is uint8_t everywhere in
    // the protocol, so this one's worth special-casing.
    inline Field field(std::string_view key, uint8_t value) {
        return Field{std::string(key), std::to_string(static_cast<unsigned>(value))};
    }

    namespace detail {

        inline Level &min_level() {
            static Level lvl = Level::Debug;   // show everything by default, matches today's "everything prints" behavior
            return lvl;
        }

        inline std::mutex &out_mutex() {
            static std::mutex m;
            return m;
        }

        inline const char *level_name(Level lvl) {
            switch (lvl) {
                case Level::Debug: return "DEBUG";
                case Level::Info:  return "INFO";
                case Level::Warn:  return "WARN";
                case Level::Error: return "ERROR";
            }
            return "UNKNOWN";
        }

        inline std::string timestamp_now() {
            using namespace std::chrono;

            auto now = system_clock::now();
            auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
            std::time_t t = system_clock::to_time_t(now);

            std::tm tm_buf{};
            gmtime_r(&t, &tm_buf);

            std::ostringstream oss;
            oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
            oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
            return oss.str();
        }

    }   // namespace detail

    // sets the minimum level that actually gets written - anything below
    // this is dropped before we even touch the mutex/streams. no CLI
    // wiring for this yet, it's here so main() can opt into it later
    // without another header change.
    inline void set_min_level(Level lvl) {
        detail::min_level() = lvl;
    }

    inline void write(Level level, std::string_view msg, std::initializer_list<Field> fields = {}) {
        if (level < detail::min_level()) {
            return;
        }

        std::ostringstream line;
        line << detail::timestamp_now() << " [" << detail::level_name(level) << "] " << msg;
        for (const Field &f : fields) {
            line << ' ' << f.key << '=' << f.value;
        }
        line << '\n';

        std::ostream &stream = (level == Level::Warn || level == Level::Error) ? std::cerr : std::cout;

        std::lock_guard<std::mutex> lock(detail::out_mutex());
        stream << line.str();
    }

    inline void debug(std::string_view msg, std::initializer_list<Field> fields = {}) { write(Level::Debug, msg, fields); }
    inline void info (std::string_view msg, std::initializer_list<Field> fields = {}) { write(Level::Info,  msg, fields); }
    inline void warn (std::string_view msg, std::initializer_list<Field> fields = {}) { write(Level::Warn,  msg, fields); }
    inline void error(std::string_view msg, std::initializer_list<Field> fields = {}) { write(Level::Error, msg, fields); }

}   // namespace m0rtis::log
