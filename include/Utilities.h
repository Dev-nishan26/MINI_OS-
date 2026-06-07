#pragma once

#include <chrono>
#include <ctime>
#include <functional>
#include <iomanip>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace util {

std::string trim(const std::string& value);
std::string toLower(std::string value);
std::vector<std::string> split(const std::string& value, char delimiter);
std::string join(const std::vector<std::string>& parts, const std::string& delimiter);
std::vector<std::string> tokenize(const std::string& line);
std::string prompt(const std::string& label);
std::string promptMultiline(const std::string& label, const std::string& sentinel = ".");
bool confirm(const std::string& label);
void pause();
void clearScreen();
std::string repeat(char ch, int count);
std::string center(const std::string& text, int width);
std::string nowDateTime();
std::string currentDate();
std::string currentTime();
std::string timeToString(std::time_t value, const char* format = "%Y-%m-%d %H:%M");
std::optional<std::time_t> parseDateTime(const std::string& value);
std::optional<std::time_t> parseDate(const std::string& value);
std::tm safeLocalTime(std::time_t value);
std::tm safeGmTime(std::time_t value);
long long daysBetween(const std::string& earlierDate, const std::string& laterDate);
int daysInMonth(int year, int month);
int weekday(int year, int month, int day);
void printMonthCalendar(int year, int month);
std::string escapeField(const std::string& value);
std::string unescapeField(const std::string& value);
std::vector<std::string> parseFields(const std::string& value);
std::string randomToken(std::size_t length = 12);
std::string hashPassword(const std::string& password, const std::string& salt);
std::string quote(const std::string& value);
std::string shorten(const std::string& value, std::size_t maxLength);

template <typename T>
T promptNumber(const std::string& label, T minValue, T maxValue) {
    while (true) {
        std::string raw = prompt(label);
        std::stringstream stream(raw);
        T parsed {};
        char extra {};
        if ((stream >> parsed) && !(stream >> extra) && parsed >= minValue && parsed <= maxValue) {
            return parsed;
        }
        std::cout << "Enter a value between " << minValue << " and " << maxValue << ".\n";
    }
}

template <typename T, typename Predicate>
std::vector<T> filterCopy(const std::vector<T>& items, Predicate predicate) {
    std::vector<T> result;
    for (const auto& item : items) {
        if (predicate(item)) {
            result.push_back(item);
        }
    }
    return result;
}

template <typename T>
const T& randomChoice(const std::vector<T>& items) {
    static std::mt19937 generator(static_cast<unsigned int>(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<std::size_t> distribution(0, items.size() - 1);
    return items[distribution(generator)];
}

}  // namespace util
