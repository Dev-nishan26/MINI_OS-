#include "Utilities.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <stdexcept>

namespace util {

std::string trim(const std::string& value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(start, end - start);
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::vector<std::string> split(const std::string& value, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, delimiter)) {
        parts.push_back(item);
    }
    return parts;
}

std::string join(const std::vector<std::string>& parts, const std::string& delimiter) {
    std::ostringstream output;
    for (std::size_t index = 0; index < parts.size(); ++index) {
        if (index > 0) {
            output << delimiter;
        }
        output << parts[index];
    }
    return output.str();
}

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes = false;
    for (char ch : line) {
        if (ch == '"') {
            inQuotes = !inQuotes;
            continue;
        }
        if (std::isspace(static_cast<unsigned char>(ch)) && !inQuotes) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

std::string prompt(const std::string& label) {
    std::cout << label;
    std::string line;
    std::getline(std::cin, line);
    return trim(line);
}

std::string promptMultiline(const std::string& label, const std::string& sentinel) {
    std::cout << label << " End with " << quote(sentinel) << ".\n";
    std::string line;
    std::ostringstream body;
    bool first = true;
    while (std::getline(std::cin, line)) {
        if (trim(line) == sentinel) {
            break;
        }
        if (!first) {
            body << '\n';
        }
        body << line;
        first = false;
    }
    return body.str();
}

bool confirm(const std::string& label) {
    std::string answer = toLower(prompt(label + " (y/n): "));
    return answer == "y" || answer == "yes";
}

void pause() {
    std::cout << "Press Enter to continue...";
    std::string discard;
    std::getline(std::cin, discard);
}

void clearScreen() {
#ifdef _WIN32
    std::system("cls");
#else
    std::system("clear");
#endif
}

std::string repeat(char ch, int count) {
    if (count <= 0) {
        return {};
    }
    return std::string(static_cast<std::size_t>(count), ch);
}

std::string center(const std::string& text, int width) {
    if (static_cast<int>(text.size()) >= width) {
        return text;
    }
    int padding = (width - static_cast<int>(text.size())) / 2;
    return repeat(' ', padding) + text;
}

std::string nowDateTime() {
    return timeToString(std::time(nullptr));
}

std::string currentDate() {
    return timeToString(std::time(nullptr), "%Y-%m-%d");
}

std::string currentTime() {
    return timeToString(std::time(nullptr), "%H:%M:%S");
}

std::string timeToString(std::time_t value, const char* format) {
    std::tm local = safeLocalTime(value);
    std::ostringstream output;
    output << std::put_time(&local, format);
    return output.str();
}

std::optional<std::time_t> parseDateTime(const std::string& value) {
    std::tm parsed {};
    std::istringstream input(value);
    input >> std::get_time(&parsed, "%Y-%m-%d %H:%M");
    if (input.fail()) {
        return std::nullopt;
    }
    parsed.tm_isdst = -1;
    return std::mktime(&parsed);
}

std::optional<std::time_t> parseDate(const std::string& value) {
    std::tm parsed {};
    std::istringstream input(value);
    input >> std::get_time(&parsed, "%Y-%m-%d");
    if (input.fail()) {
        return std::nullopt;
    }
    parsed.tm_isdst = -1;
    parsed.tm_hour = 0;
    parsed.tm_min = 0;
    parsed.tm_sec = 0;
    return std::mktime(&parsed);
}

std::tm safeLocalTime(std::time_t value) {
    std::tm result {};
#ifdef _WIN32
    localtime_s(&result, &value);
#else
    localtime_r(&value, &result);
#endif
    return result;
}

std::tm safeGmTime(std::time_t value) {
    std::tm result {};
#ifdef _WIN32
    gmtime_s(&result, &value);
#else
    gmtime_r(&value, &result);
#endif
    return result;
}

long long daysBetween(const std::string& earlierDate, const std::string& laterDate) {
    auto earlier = parseDate(earlierDate);
    auto later = parseDate(laterDate);
    if (!earlier || !later) {
        return std::numeric_limits<long long>::max();
    }
    constexpr long long secondsPerDay = 60LL * 60LL * 24LL;
    return static_cast<long long>((*later - *earlier) / secondsPerDay);
}

int daysInMonth(int year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2) {
        bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return leap ? 29 : 28;
    }
    return days[month - 1];
}

int weekday(int year, int month, int day) {
    std::tm date {};
    date.tm_year = year - 1900;
    date.tm_mon = month - 1;
    date.tm_mday = day;
    std::mktime(&date);
    return date.tm_wday;
}

void printMonthCalendar(int year, int month) {
    static const char* names[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    std::cout << names[month - 1] << ' ' << year << "\n";
    std::cout << "Su Mo Tu We Th Fr Sa\n";
    int start = weekday(year, month, 1);
    int days = daysInMonth(year, month);
    for (int i = 0; i < start; ++i) {
        std::cout << "   ";
    }
    for (int day = 1; day <= days; ++day) {
        std::cout << std::setw(2) << day << ' ';
        if ((start + day) % 7 == 0) {
            std::cout << '\n';
        }
    }
    std::cout << "\n";
}

std::string escapeField(const std::string& value) {
    std::string result;
    for (char ch : value) {
        if (ch == '\\' || ch == '|') {
            result.push_back('\\');
        }
        result.push_back(ch);
    }
    return result;
}

std::string unescapeField(const std::string& value) {
    std::string result;
    bool escaping = false;
    for (char ch : value) {
        if (escaping) {
            result.push_back(ch);
            escaping = false;
        } else if (ch == '\\') {
            escaping = true;
        } else {
            result.push_back(ch);
        }
    }
    return result;
}

std::vector<std::string> parseFields(const std::string& value) {
    std::vector<std::string> fields;
    std::string current;
    bool escaping = false;
    for (char ch : value) {
        if (escaping) {
            current.push_back(ch);
            escaping = false;
        } else if (ch == '\\') {
            escaping = true;
        } else if (ch == '|') {
            fields.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    fields.push_back(current);
    return fields;
}

std::string randomToken(std::size_t length) {
    static const std::string alphabet =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static std::mt19937 generator(static_cast<unsigned int>(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<std::size_t> distribution(0, alphabet.size() - 1);

    std::string token;
    token.reserve(length);
    for (std::size_t i = 0; i < length; ++i) {
        token.push_back(alphabet[distribution(generator)]);
    }
    return token;
}

std::string hashPassword(const std::string& password, const std::string& salt) {
    std::hash<std::string> hasher;
    std::size_t round1 = hasher(password + "::" + salt);
    std::size_t round2 = hasher(salt + "::" + std::to_string(round1));
    return std::to_string(round2);
}

std::string quote(const std::string& value) {
    return "\"" + value + "\"";
}

std::string shorten(const std::string& value, std::size_t maxLength) {
    if (value.size() <= maxLength) {
        return value;
    }
    if (maxLength <= 3) {
        return value.substr(0, maxLength);
    }
    return value.substr(0, maxLength - 3) + "...";
}

}  // namespace util
