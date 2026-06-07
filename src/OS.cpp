#include "OS.h"

#include "Utilities.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

std::string lineBar(int width = 78) {
    return util::repeat('-', width);
}

void printStandaloneTitle(const std::string& title, const std::string& subtitle = {}) {
    util::clearScreen();
    std::cout << "SMAAT\n";
    std::cout << "Smart Modular Automation Terminal with Intelligence Layer\n";
    std::cout << lineBar() << "\n";
    std::cout << title << "\n";
    if (!subtitle.empty()) {
        std::cout << subtitle << "\n";
    }
    std::cout << lineBar() << "\n";
}

void printAppTitle(const std::string& title,
                   const std::optional<User>& user,
                   const std::string& subtitle = {}) {
    util::clearScreen();
    std::cout << "SMAAT | Smart Modular Automation Terminal with Intelligence Layer\n";
    std::cout << lineBar() << "\n";
    std::cout << title;
    if (user) {
        std::cout << " | Operator: " << user->username;
    }
    std::cout << "\n";
    std::cout << "Time: " << util::nowDateTime() << "\n";
    if (!subtitle.empty()) {
        std::cout << subtitle << "\n";
    }
    std::cout << lineBar() << "\n";
}

void printMenu(const std::vector<std::string>& items) {
    for (const auto& item : items) {
        std::cout << item << "\n";
    }
}

void printKeyValue(const std::string& key, const std::string& value) {
    std::cout << std::left << std::setw(18) << key << value << "\n";
}

std::string userFile(const std::string& username, const std::string& suffix) {
    return username + "_" + suffix;
}

bool isSimpleUsername(const std::string& value) {
    if (value.size() < 3) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return std::isalnum(ch) || ch == '_';
    });
}

bool launchExternalFile(const std::string& path) {
    if (path.empty()) {
        std::cout << "No file path is stored for this item.\n";
        return false;
    }
    if (!std::filesystem::exists(path)) {
        std::cout << "That file path does not exist on disk.\n";
        return false;
    }
#ifdef _WIN32
    std::string command = "cmd /c start \"\" \"" + path + "\"";
#elif __APPLE__
    std::string command = "open \"" + path + "\"";
#else
    std::string command = "xdg-open \"" + path + "\"";
#endif
    return std::system(command.c_str()) == 0;
}

std::string moodDirective(int openTodos, int streak) {
    if (openTodos >= 8 && streak >= 5) {
        return "High load, high resilience. Protect the streak and complete the single hardest task first.";
    }
    if (openTodos >= 8) {
        return "The board is crowded. Narrow your attention to two tasks and finish one before switching.";
    }
    if (streak >= 5) {
        return "Momentum is carrying you. Keep the streak alive and close one meaningful task today.";
    }
    return "The system is calm enough to build. Pick one quick win and then move into focused work.";
}

class ExpressionParser {
public:
    ExpressionParser(std::string expression, double lastAnswer)
        : expression_(std::move(expression)), lastAnswer_(lastAnswer) {}

    double parse() {
        index_ = 0;
        double result = parseExpression();
        skipWhitespace();
        if (index_ != expression_.size()) {
            throw std::runtime_error("Unexpected token near: " + expression_.substr(index_));
        }
        return result;
    }

private:
    double parseExpression() {
        double value = parseTerm();
        while (true) {
            skipWhitespace();
            if (match('+')) {
                value += parseTerm();
            } else if (match('-')) {
                value -= parseTerm();
            } else {
                break;
            }
        }
        return value;
    }

    double parseTerm() {
        double value = parsePower();
        while (true) {
            skipWhitespace();
            if (match('*')) {
                value *= parsePower();
            } else if (match('/')) {
                double divisor = parsePower();
                if (std::abs(divisor) < 1e-12) {
                    throw std::runtime_error("Division by zero");
                }
                value /= divisor;
            } else if (match('%')) {
                double divisor = parsePower();
                if (std::abs(divisor) < 1e-12) {
                    throw std::runtime_error("Division by zero");
                }
                value = std::fmod(value, divisor);
            } else {
                break;
            }
        }
        return value;
    }

    double parsePower() {
        double value = parseUnary();
        skipWhitespace();
        if (match('^')) {
            return std::pow(value, parsePower());
        }
        return value;
    }

    double parseUnary() {
        skipWhitespace();
        if (match('+')) {
            return parseUnary();
        }
        if (match('-')) {
            return -parseUnary();
        }
        return parsePrimary();
    }

    double parsePrimary() {
        skipWhitespace();
        if (match('(')) {
            double value = parseExpression();
            expect(')');
            return value;
        }
        if (std::isdigit(peek()) || peek() == '.') {
            return parseNumber();
        }
        if (std::isalpha(peek())) {
            std::string name = parseIdentifier();
            skipWhitespace();
            if (match('(')) {
                double argument = parseExpression();
                expect(')');
                return applyFunction(name, argument);
            }
            if (name == "pi") {
                return 3.14159265358979323846;
            }
            if (name == "e") {
                return 2.71828182845904523536;
            }
            if (name == "ans") {
                return lastAnswer_;
            }
            throw std::runtime_error("Unknown symbol: " + name);
        }
        throw std::runtime_error("Expected a number, a variable, or a function.");
    }

    double parseNumber() {
        std::size_t consumed = 0;
        double value = std::stod(expression_.substr(index_), &consumed);
        index_ += consumed;
        return value;
    }

    std::string parseIdentifier() {
        std::string name;
        while (index_ < expression_.size() &&
               (std::isalpha(static_cast<unsigned char>(expression_[index_])) ||
                std::isdigit(static_cast<unsigned char>(expression_[index_])) ||
                expression_[index_] == '_')) {
            name.push_back(static_cast<char>(std::tolower(expression_[index_])));
            ++index_;
        }
        return name;
    }

    double applyFunction(const std::string& name, double value) const {
        if (name == "sqrt") {
            if (value < 0) {
                throw std::runtime_error("sqrt() needs a non-negative value");
            }
            return std::sqrt(value);
        }
        if (name == "sin") {
            return std::sin(value);
        }
        if (name == "cos") {
            return std::cos(value);
        }
        if (name == "tan") {
            return std::tan(value);
        }
        if (name == "log") {
            if (value <= 0) {
                throw std::runtime_error("log() needs a positive value");
            }
            return std::log(value);
        }
        if (name == "abs") {
            return std::abs(value);
        }
        throw std::runtime_error("Unknown function: " + name);
    }

    void skipWhitespace() {
        while (index_ < expression_.size() &&
               std::isspace(static_cast<unsigned char>(expression_[index_]))) {
            ++index_;
        }
    }

    char peek() const {
        if (index_ >= expression_.size()) {
            return '\0';
        }
        return expression_[index_];
    }

    bool match(char token) {
        if (peek() == token) {
            ++index_;
            return true;
        }
        return false;
    }

    void expect(char token) {
        if (!match(token)) {
            std::string message = "Expected '";
            message.push_back(token);
            message.push_back('\'');
            throw std::runtime_error(message);
        }
    }

    std::string expression_;
    std::size_t index_ {};
    double lastAnswer_ {};
};

}  // namespace

UserManager::UserManager(DataStore& store) : store_(store) {
    load();
}

bool UserManager::registerUser() {
    printStandaloneTitle("Create SMAAT Identity", "Set up an operator profile for this terminal.");
    std::string username = util::prompt("Operator name (letters, numbers, underscore): ");
    if (!isSimpleUsername(username)) {
        std::cout << "Use at least 3 characters and stick to letters, numbers, or underscore.\n";
        util::pause();
        return false;
    }
    if (usernameExists(username)) {
        std::cout << "That operator name is already in use.\n";
        util::pause();
        return false;
    }

    std::string password = util::prompt("Access key: ");
    std::string confirm = util::prompt("Confirm access key: ");
    if (password.size() < 4) {
        std::cout << "Use an access key with at least 4 characters.\n";
        util::pause();
        return false;
    }
    if (password != confirm) {
        std::cout << "The access keys did not match.\n";
        util::pause();
        return false;
    }

    User user;
    user.username = username;
    user.salt = util::randomToken();
    user.passwordHash = util::hashPassword(password, user.salt);
    user.motto = util::prompt("Operator motto (optional): ");
    user.createdAt = util::nowDateTime();

    users_.push_back(user);
    save();
    std::cout << "SMAAT identity created. You can sign in now.\n";
    util::pause();
    return true;
}

std::optional<User> UserManager::login() {
    printStandaloneTitle("SMAAT Access Portal", "Authenticate to enter the application launcher.");
    std::string username = util::prompt("Operator name: ");
    std::string password = util::prompt("Access key: ");

    for (const auto& user : users_) {
        if (user.username != username) {
            continue;
        }
        if (user.passwordHash == util::hashPassword(password, user.salt)) {
            return user;
        }
        std::cout << "Access denied. The key did not match.\n";
        util::pause();
        return std::nullopt;
    }

    std::cout << "No SMAAT identity exists with that operator name.\n";
    util::pause();
    return std::nullopt;
}

void UserManager::updateUser(const User& user) {
    for (auto& existing : users_) {
        if (existing.username == user.username) {
            existing = user;
            save();
            return;
        }
    }
}

bool UserManager::changePassword(User& user) {
    std::string oldPassword = util::prompt("Current access key: ");
    if (user.passwordHash != util::hashPassword(oldPassword, user.salt)) {
        std::cout << "Current access key is incorrect.\n";
        util::pause();
        return false;
    }
    std::string nextPassword = util::prompt("New access key: ");
    std::string confirm = util::prompt("Confirm new access key: ");
    if (nextPassword.size() < 4 || nextPassword != confirm) {
        std::cout << "The new access key setup failed.\n";
        util::pause();
        return false;
    }
    user.salt = util::randomToken();
    user.passwordHash = util::hashPassword(nextPassword, user.salt);
    updateUser(user);
    std::cout << "Access key updated.\n";
    util::pause();
    return true;
}

void UserManager::load() {
    users_ = store_.loadList<User>("users.db");
}

void UserManager::save() const {
    store_.saveList("users.db", users_);
}

bool UserManager::usernameExists(const std::string& username) const {
    return std::any_of(users_.begin(), users_.end(), [&](const User& user) {
        return user.username == username;
    });
}

TodoManager::TodoManager(const DataStore& store) : repo_(store) {}

void TodoManager::bindUser(const std::string& username) {
    repo_.useFile(userFile(username, "todos.db"));
}

void TodoManager::addInteractive() {
    Todo todo;
    todo.id = repo_.nextId();
    todo.title = util::prompt("Task title: ");
    if (todo.title.empty()) {
        std::cout << "Task title cannot be empty.\n";
        return;
    }
    todo.category = util::prompt("Category or project: ");
    todo.dueDate = util::prompt("Due date (YYYY-MM-DD, optional): ");
    if (!todo.dueDate.empty() && !util::parseDate(todo.dueDate)) {
        std::cout << "The due date must use YYYY-MM-DD.\n";
        return;
    }
    todo.priority = util::promptNumber<int>("Priority 1-5: ", 1, 5);
    todo.done = false;
    todo.createdAt = util::nowDateTime();
    repo_.items().push_back(todo);
    repo_.save();
    std::cout << "Task added to the Notes Suite.\n";
}

void TodoManager::list() const {
    if (repo_.items().empty()) {
        std::cout << "No tasks saved.\n";
        return;
    }
    auto copy = repo_.items();
    std::sort(copy.begin(), copy.end(), [](const Todo& left, const Todo& right) {
        if (left.done != right.done) {
            return left.done < right.done;
        }
        if (left.priority != right.priority) {
            return left.priority > right.priority;
        }
        return left.id < right.id;
    });

    std::cout << std::left << std::setw(4) << "ID"
              << std::setw(8) << "State"
              << std::setw(5) << "P"
              << std::setw(14) << "Due"
              << std::setw(18) << "Category"
              << "Task\n";
    std::cout << lineBar() << "\n";
    for (const auto& todo : copy) {
        std::cout << std::left << std::setw(4) << todo.id
                  << std::setw(8) << (todo.done ? "done" : "open")
                  << std::setw(5) << todo.priority
                  << std::setw(14) << (todo.dueDate.empty() ? "-" : todo.dueDate)
                  << std::setw(18) << util::shorten(todo.category.empty() ? "general" : todo.category, 17)
                  << todo.title << "\n";
    }
}

bool TodoManager::markDone(int id) {
    for (auto& todo : repo_.items()) {
        if (todo.id == id) {
            todo.done = true;
            repo_.save();
            std::cout << "Task marked complete.\n";
            return true;
        }
    }
    return false;
}

bool TodoManager::remove(int id) {
    auto& items = repo_.items();
    auto before = items.size();
    items.erase(std::remove_if(items.begin(), items.end(), [&](const Todo& todo) {
        return todo.id == id;
    }), items.end());
    if (items.size() != before) {
        repo_.save();
        std::cout << "Task removed.\n";
        return true;
    }
    return false;
}

int TodoManager::openCount() const {
    return static_cast<int>(std::count_if(repo_.items().begin(), repo_.items().end(), [](const Todo& todo) {
        return !todo.done;
    }));
}

int TodoManager::completedCount() const {
    return static_cast<int>(std::count_if(repo_.items().begin(), repo_.items().end(), [](const Todo& todo) {
        return todo.done;
    }));
}

int TodoManager::count() const {
    return static_cast<int>(repo_.items().size());
}

const std::vector<Todo>& TodoManager::items() const {
    return repo_.items();
}

std::optional<Todo> TodoManager::topPriorityOpen() const {
    std::optional<Todo> best;
    for (const auto& todo : repo_.items()) {
        if (todo.done) {
            continue;
        }
        if (!best || todo.priority > best->priority) {
            best = todo;
        }
    }
    return best;
}

VaultManager::VaultManager(const DataStore& store) : repo_(store) {}

void VaultManager::bindUser(const std::string& username) {
    repo_.useFile(userFile(username, "vault.db"));
}

void VaultManager::addInteractive() {
    Note note;
    note.id = repo_.nextId();
    note.title = util::prompt("Note title: ");
    if (note.title.empty()) {
        std::cout << "Note title cannot be empty.\n";
        return;
    }
    note.tags = util::prompt("Tags (comma separated): ");
    note.body = util::promptMultiline("Write the note body.");
    note.createdAt = util::nowDateTime();
    repo_.items().push_back(note);
    repo_.save();
    std::cout << "Note saved to the vault.\n";
}

void VaultManager::list() const {
    if (repo_.items().empty()) {
        std::cout << "No notes are stored yet.\n";
        return;
    }
    for (const auto& note : repo_.items()) {
        std::cout << "#" << note.id << "  "
                  << note.title << "  ["
                  << (note.tags.empty() ? "untagged" : note.tags)
                  << "]  " << note.createdAt << "\n";
    }
}

bool VaultManager::read(int id) const {
    for (const auto& note : repo_.items()) {
        if (note.id == id) {
            std::cout << lineBar() << "\n";
            std::cout << note.title << "\n";
            std::cout << "Tags: " << (note.tags.empty() ? "-" : note.tags) << "\n";
            std::cout << "Created: " << note.createdAt << "\n";
            std::cout << lineBar() << "\n";
            std::cout << note.body << "\n";
            return true;
        }
    }
    return false;
}

bool VaultManager::remove(int id) {
    auto& items = repo_.items();
    auto before = items.size();
    items.erase(std::remove_if(items.begin(), items.end(), [&](const Note& note) {
        return note.id == id;
    }), items.end());
    if (items.size() != before) {
        repo_.save();
        std::cout << "Note removed.\n";
        return true;
    }
    return false;
}

void VaultManager::search(const std::string& query) const {
    std::string needle = util::toLower(query);
    bool found = false;
    for (const auto& note : repo_.items()) {
        std::string merged = util::toLower(note.title + "\n" + note.body + "\n" + note.tags);
        if (merged.find(needle) != std::string::npos) {
            found = true;
            std::cout << "#" << note.id << " " << note.title << " [" << note.tags << "]\n";
        }
    }
    if (!found) {
        std::cout << "No notes matched that search.\n";
    }
}

int VaultManager::count() const {
    return static_cast<int>(repo_.items().size());
}

std::optional<Note> VaultManager::latest() const {
    if (repo_.items().empty()) {
        return std::nullopt;
    }
    return repo_.items().back();
}

HabitManager::HabitManager(const DataStore& store) : repo_(store) {}

void HabitManager::bindUser(const std::string& username) {
    repo_.useFile(userFile(username, "habits.db"));
}

void HabitManager::addInteractive() {
    Habit habit;
    habit.id = repo_.nextId();
    habit.name = util::prompt("Habit name: ");
    if (habit.name.empty()) {
        std::cout << "Habit name cannot be empty.\n";
        return;
    }
    habit.cadence = util::toLower(util::prompt("Cadence (daily/weekly): "));
    if (habit.cadence != "daily" && habit.cadence != "weekly") {
        habit.cadence = "daily";
    }
    habit.streak = 0;
    habit.totalCheckins = 0;
    repo_.items().push_back(habit);
    repo_.save();
    std::cout << "Habit added.\n";
}

void HabitManager::list() const {
    if (repo_.items().empty()) {
        std::cout << "No habits set yet.\n";
        return;
    }
    std::cout << std::left << std::setw(4) << "ID"
              << std::setw(24) << "Habit"
              << std::setw(10) << "Cadence"
              << std::setw(8) << "Streak"
              << std::setw(14) << "Last"
              << "Total\n";
    std::cout << lineBar() << "\n";
    for (const auto& habit : repo_.items()) {
        std::cout << std::left << std::setw(4) << habit.id
                  << std::setw(24) << util::shorten(habit.name, 23)
                  << std::setw(10) << habit.cadence
                  << std::setw(8) << habit.streak
                  << std::setw(14) << (habit.lastCheckin.empty() ? "-" : habit.lastCheckin)
                  << habit.totalCheckins << "\n";
    }
}

bool HabitManager::checkIn(int id) {
    std::string today = util::currentDate();
    for (auto& habit : repo_.items()) {
        if (habit.id != id) {
            continue;
        }
        if (habit.lastCheckin == today) {
            std::cout << "Already checked in today.\n";
            return true;
        }
        if (habit.lastCheckin.empty()) {
            habit.streak = 1;
        } else {
            long long gap = util::daysBetween(habit.lastCheckin, today);
            if (habit.cadence == "daily") {
                habit.streak = (gap == 1) ? habit.streak + 1 : 1;
            } else {
                habit.streak = (gap >= 1 && gap <= 7) ? habit.streak + 1 : 1;
            }
        }
        habit.lastCheckin = today;
        ++habit.totalCheckins;
        repo_.save();
        std::cout << "Habit checked in. Current streak: " << habit.streak << "\n";
        return true;
    }
    return false;
}

bool HabitManager::remove(int id) {
    auto& items = repo_.items();
    auto before = items.size();
    items.erase(std::remove_if(items.begin(), items.end(), [&](const Habit& habit) {
        return habit.id == id;
    }), items.end());
    if (items.size() != before) {
        repo_.save();
        std::cout << "Habit removed.\n";
        return true;
    }
    return false;
}

int HabitManager::strongestStreak() const {
    int best = 0;
    for (const auto& habit : repo_.items()) {
        best = std::max(best, habit.streak);
    }
    return best;
}

int HabitManager::count() const {
    return static_cast<int>(repo_.items().size());
}

AlarmManager::AlarmManager(const DataStore& store) : repo_(store) {}

void AlarmManager::bindUser(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    repo_.useFile(userFile(username, "alarms.db"));
}

void AlarmManager::addInteractive() {
    Alarm alarm;
    alarm.id = repo_.nextId();
    alarm.label = util::prompt("Alarm label: ");
    auto parsed = util::parseDateTime(util::prompt("Trigger time (YYYY-MM-DD HH:MM): "));
    if (!parsed) {
        std::cout << "Time must use YYYY-MM-DD HH:MM.\n";
        return;
    }
    alarm.trigger = *parsed;
    alarm.fired = false;
    alarm.createdAt = util::nowDateTime();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        repo_.items().push_back(alarm);
        repo_.save();
    }
    std::cout << "Alarm scheduled.\n";
}

void AlarmManager::list() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (repo_.items().empty()) {
        std::cout << "No alarms are scheduled.\n";
        return;
    }
    for (const auto& alarm : repo_.items()) {
        std::cout << "#" << alarm.id << "  "
                  << util::timeToString(alarm.trigger)
                  << "  "
                  << (alarm.fired ? "[fired] " : "[pending] ")
                  << alarm.label << "\n";
    }
}

bool AlarmManager::remove(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& items = repo_.items();
    auto before = items.size();
    items.erase(std::remove_if(items.begin(), items.end(), [&](const Alarm& alarm) {
        return alarm.id == id;
    }), items.end());
    if (items.size() != before) {
        repo_.save();
        std::cout << "Alarm removed.\n";
        return true;
    }
    return false;
}

std::vector<Alarm> AlarmManager::collectDue() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Alarm> due;
    std::time_t now = std::time(nullptr);
    for (auto& alarm : repo_.items()) {
        if (!alarm.fired && alarm.trigger <= now) {
            alarm.fired = true;
            due.push_back(alarm);
        }
    }
    if (!due.empty()) {
        repo_.save();
    }
    return due;
}

int AlarmManager::pendingCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(std::count_if(repo_.items().begin(), repo_.items().end(), [](const Alarm& alarm) {
        return !alarm.fired;
    }));
}

int AlarmManager::count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(repo_.items().size());
}

std::optional<Alarm> AlarmManager::nextAlarm() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::optional<Alarm> next;
    for (const auto& alarm : repo_.items()) {
        if (alarm.fired) {
            continue;
        }
        if (!next || alarm.trigger < next->trigger) {
            next = alarm;
        }
    }
    return next;
}

MusicManager::MusicManager(const DataStore& store) : repo_(store) {}

void MusicManager::bindUser(const std::string& username) {
    repo_.useFile(userFile(username, "songs.db"));
}

void MusicManager::addInteractive() {
    Song song;
    song.id = repo_.nextId();
    song.title = util::prompt("Track title: ");
    if (song.title.empty()) {
        std::cout << "Track title cannot be empty.\n";
        return;
    }
    song.artist = util::prompt("Artist: ");
    song.path = util::prompt("Audio file path: ");
    song.mood = util::prompt("Mood or playlist tag: ");
    song.playCount = 0;
    song.addedAt = util::nowDateTime();
    repo_.items().push_back(song);
    repo_.save();
    std::cout << "Track added to the library.\n";
}

void MusicManager::list() const {
    if (repo_.items().empty()) {
        std::cout << "Music library is empty.\n";
        return;
    }
    std::cout << std::left << std::setw(4) << "ID"
              << std::setw(24) << "Title"
              << std::setw(18) << "Artist"
              << std::setw(16) << "Mood"
              << "Plays\n";
    std::cout << lineBar() << "\n";
    for (const auto& song : repo_.items()) {
        std::cout << std::left << std::setw(4) << song.id
                  << std::setw(24) << util::shorten(song.title, 23)
                  << std::setw(18) << util::shorten(song.artist.empty() ? "-" : song.artist, 17)
                  << std::setw(16) << util::shorten(song.mood.empty() ? "-" : song.mood, 15)
                  << song.playCount << "\n";
    }
}

bool MusicManager::play(int id) {
    for (auto& song : repo_.items()) {
        if (song.id == id) {
            ++song.playCount;
            repo_.save();
            std::cout << "Opening " << song.title << ".\n";
            return launchExternalFile(song.path);
        }
    }
    return false;
}

bool MusicManager::shufflePlay() {
    if (repo_.items().empty()) {
        return false;
    }
    static std::mt19937 generator(static_cast<unsigned int>(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<std::size_t> distribution(0, repo_.items().size() - 1);
    Song& song = repo_.items()[distribution(generator)];
    ++song.playCount;
    repo_.save();
    std::cout << "Shuffle picked " << song.title << ".\n";
    return launchExternalFile(song.path);
}

bool MusicManager::remove(int id) {
    auto& items = repo_.items();
    auto before = items.size();
    items.erase(std::remove_if(items.begin(), items.end(), [&](const Song& song) {
        return song.id == id;
    }), items.end());
    if (items.size() != before) {
        repo_.save();
        std::cout << "Track removed.\n";
        return true;
    }
    return false;
}

int MusicManager::count() const {
    return static_cast<int>(repo_.items().size());
}

std::optional<Song> MusicManager::favorite() const {
    if (repo_.items().empty()) {
        return std::nullopt;
    }
    return *std::max_element(repo_.items().begin(), repo_.items().end(), [](const Song& left, const Song& right) {
        return left.playCount < right.playCount;
    });
}

VideoManager::VideoManager(const DataStore& store) : repo_(store) {}

void VideoManager::bindUser(const std::string& username) {
    repo_.useFile(userFile(username, "videos.db"));
}

void VideoManager::addInteractive() {
    VideoClip video;
    video.id = repo_.nextId();
    video.title = util::prompt("Video title: ");
    if (video.title.empty()) {
        std::cout << "Video title cannot be empty.\n";
        return;
    }
    video.category = util::prompt("Category: ");
    video.path = util::prompt("Video file path: ");
    video.watchCount = 0;
    video.addedAt = util::nowDateTime();
    repo_.items().push_back(video);
    repo_.save();
    std::cout << "Video added to the library.\n";
}

void VideoManager::list() const {
    if (repo_.items().empty()) {
        std::cout << "Video library is empty.\n";
        return;
    }
    std::cout << std::left << std::setw(4) << "ID"
              << std::setw(28) << "Title"
              << std::setw(20) << "Category"
              << "Views\n";
    std::cout << lineBar() << "\n";
    for (const auto& video : repo_.items()) {
        std::cout << std::left << std::setw(4) << video.id
                  << std::setw(28) << util::shorten(video.title, 27)
                  << std::setw(20) << util::shorten(video.category.empty() ? "-" : video.category, 19)
                  << video.watchCount << "\n";
    }
}

bool VideoManager::play(int id) {
    for (auto& video : repo_.items()) {
        if (video.id == id) {
            ++video.watchCount;
            repo_.save();
            std::cout << "Opening " << video.title << ".\n";
            return launchExternalFile(video.path);
        }
    }
    return false;
}

bool VideoManager::remove(int id) {
    auto& items = repo_.items();
    auto before = items.size();
    items.erase(std::remove_if(items.begin(), items.end(), [&](const VideoClip& video) {
        return video.id == id;
    }), items.end());
    if (items.size() != before) {
        repo_.save();
        std::cout << "Video removed.\n";
        return true;
    }
    return false;
}

int VideoManager::count() const {
    return static_cast<int>(repo_.items().size());
}

CalculatorManager::CalculatorManager(const DataStore& store) : repo_(store) {}

void CalculatorManager::bindUser(const std::string& username) {
    repo_.useFile(userFile(username, "calc.db"));
}

void CalculatorManager::repl() {
    std::cout << "Expression mode is live. Functions: sqrt, sin, cos, tan, log, abs. Use ans for the last result.\n";
    std::cout << "Submit a blank line to leave expression mode.\n";
    while (true) {
        std::string expression = util::prompt("calc> ");
        if (expression.empty()) {
            break;
        }
        evaluateAndPrint(expression);
    }
}

bool CalculatorManager::evaluateAndPrint(const std::string& expression) {
    try {
        ExpressionParser parser(expression, lastAnswer_);
        double result = parser.parse();
        lastAnswer_ = result;
        CalcEntry entry;
        entry.id = repo_.nextId();
        entry.expression = expression;
        entry.result = result;
        entry.createdAt = util::nowDateTime();
        repo_.items().push_back(entry);
        repo_.save();
        std::cout << std::fixed << std::setprecision(6) << result << "\n";
        std::cout.unsetf(std::ios::floatfield);
        return true;
    } catch (const std::exception& error) {
        std::cout << "Calculation error: " << error.what() << "\n";
        return false;
    }
}

void CalculatorManager::showHistory() const {
    if (repo_.items().empty()) {
        std::cout << "No calculator history yet.\n";
        return;
    }
    for (const auto& entry : repo_.items()) {
        std::cout << "#" << entry.id << "  "
                  << entry.expression << " = "
                  << std::fixed << std::setprecision(6) << entry.result
                  << "  " << entry.createdAt << "\n";
    }
    std::cout.unsetf(std::ios::floatfield);
}

FileManagerApp::FileManagerApp() : currentPath_(std::filesystem::current_path()) {}

void FileManagerApp::run() {
    while (true) {
        printStandaloneTitle("File Manager", "Browse, search, create, move, and remove files from one place.");
        std::cout << "Current location: " << currentPath_.string() << "\n";
        std::cout << lineBar() << "\n";
        showDirectory();
        std::cout << "\n";
        printMenu({
            "1. Open a folder",
            "2. Go up one level",
            "3. Preview a file",
            "4. Create a folder",
            "5. Create an empty file",
            "6. Copy file or folder",
            "7. Move or rename",
            "8. Delete file or folder",
            "9. Search by name",
            "10. Support",
            "0. Back to launcher"
        });

        std::string choice = util::prompt("Choose action: ");
        try {
            if (choice == "1") {
                openFolder();
            } else if (choice == "2") {
                goUp();
            } else if (choice == "3") {
                previewFile();
            } else if (choice == "4") {
                createFolder();
            } else if (choice == "5") {
                createFile();
            } else if (choice == "6") {
                copyItem();
            } else if (choice == "7") {
                moveItem();
            } else if (choice == "8") {
                removeItem();
            } else if (choice == "9") {
                searchItem();
            } else if (choice == "10") {
                showSupport();
            } else if (choice == "0") {
                return;
            } else {
                std::cout << "Choose one of the listed actions.\n";
                util::pause();
            }
        } catch (const std::exception& error) {
            std::cout << "File manager error: " << error.what() << "\n";
            util::pause();
        }
    }
}

std::filesystem::path FileManagerApp::resolve(const std::string& input) const {
    std::filesystem::path path(input);
    if (path.is_absolute()) {
        return path;
    }
    return currentPath_ / path;
}

void FileManagerApp::showDirectory() const {
    int shown = 0;
    for (const auto& entry : std::filesystem::directory_iterator(currentPath_)) {
        std::string label = entry.is_directory() ? "[DIR] " : "[FILE]";
        std::cout << std::left << std::setw(7) << label
                  << entry.path().filename().string();
        if (entry.is_regular_file()) {
            std::error_code error;
            auto size = std::filesystem::file_size(entry.path(), error);
            if (!error) {
                std::cout << "  (" << size << " bytes)";
            }
        }
        std::cout << "\n";
        ++shown;
        if (shown >= 20) {
            std::cout << "...showing the first 20 entries in this folder.\n";
            break;
        }
    }
    if (shown == 0) {
        std::cout << "This folder is empty.\n";
    }
}

void FileManagerApp::showSupport() const {
    printStandaloneTitle("File Manager Support", "How to move around and manage items inside SMAAT.");
    printMenu({
        "Open a folder: type the folder name or a full path to step into it.",
        "Go up one level: moves to the parent folder of the current location.",
        "Preview a file: prints the first 40 lines of a text file into the terminal.",
        "Create a folder: makes a new directory inside the current location unless you give a full path.",
        "Create an empty file: useful for quick placeholders, logs, or scratch notes.",
        "Copy file or folder: duplicates one item into another location. Folders copy recursively.",
        "Move or rename: shifts a file or folder, or simply renames it.",
        "Delete file or folder: removes an item after confirmation. Folders are deleted recursively.",
        "Search by name: scans downward from the current location and finds matching names."
    });
    util::pause();
}

void FileManagerApp::openFolder() {
    std::filesystem::path target = resolve(util::prompt("Folder to open: "));
    if (!std::filesystem::exists(target) || !std::filesystem::is_directory(target)) {
        std::cout << "That folder does not exist.\n";
        util::pause();
        return;
    }
    currentPath_ = std::filesystem::canonical(target);
}

void FileManagerApp::goUp() {
    if (currentPath_.has_parent_path()) {
        currentPath_ = currentPath_.parent_path();
    }
}

void FileManagerApp::previewFile() const {
    std::filesystem::path target = resolve(util::prompt("File to preview: "));
    if (!std::filesystem::exists(target) || std::filesystem::is_directory(target)) {
        std::cout << "That file could not be found.\n";
        util::pause();
        return;
    }
    std::ifstream input(target);
    if (!input) {
        std::cout << "The file could not be opened.\n";
        util::pause();
        return;
    }
    printStandaloneTitle("File Preview", target.string());
    std::string line;
    int count = 0;
    while (std::getline(input, line) && count < 40) {
        std::cout << line << "\n";
        ++count;
    }
    if (count == 40) {
        std::cout << lineBar() << "\nPreview stopped at 40 lines.\n";
    }
    util::pause();
}

void FileManagerApp::createFolder() const {
    std::filesystem::path target = resolve(util::prompt("Folder name or path: "));
    std::filesystem::create_directories(target);
    std::cout << "Folder created.\n";
    util::pause();
}

void FileManagerApp::createFile() const {
    std::filesystem::path target = resolve(util::prompt("File name or path: "));
    std::ofstream output(target, std::ios::app);
    std::cout << "File created or touched.\n";
    util::pause();
}

void FileManagerApp::copyItem() const {
    std::filesystem::path source = resolve(util::prompt("Source path: "));
    std::filesystem::path destination = resolve(util::prompt("Destination path: "));
    if (std::filesystem::is_directory(source)) {
        std::filesystem::copy(source, destination,
                              std::filesystem::copy_options::recursive |
                              std::filesystem::copy_options::overwrite_existing);
    } else {
        std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing);
    }
    std::cout << "Copy completed.\n";
    util::pause();
}

void FileManagerApp::moveItem() const {
    std::filesystem::path source = resolve(util::prompt("Source path: "));
    std::filesystem::path destination = resolve(util::prompt("Destination path: "));
    std::filesystem::rename(source, destination);
    std::cout << "Move completed.\n";
    util::pause();
}

void FileManagerApp::removeItem() const {
    std::filesystem::path target = resolve(util::prompt("Path to delete: "));
    if (!std::filesystem::exists(target)) {
        std::cout << "That path does not exist.\n";
        util::pause();
        return;
    }
    if (!util::confirm("Delete " + target.string() + "?")) {
        return;
    }
    if (std::filesystem::is_directory(target)) {
        std::filesystem::remove_all(target);
    } else {
        std::filesystem::remove(target);
    }
    std::cout << "Delete completed.\n";
    util::pause();
}

void FileManagerApp::searchItem() const {
    std::string query = util::toLower(util::prompt("Search text: "));
    if (query.empty()) {
        return;
    }
    printStandaloneTitle("File Search", "Matching names under " + currentPath_.string());
    int hits = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(currentPath_)) {
        std::string name = util::toLower(entry.path().filename().string());
        if (name.find(query) != std::string::npos) {
            std::cout << entry.path().string() << "\n";
            ++hits;
        }
        if (hits >= 40) {
            std::cout << lineBar() << "\nStopping after 40 matches.\n";
            break;
        }
    }
    if (hits == 0) {
        std::cout << "No matching files or folders were found.\n";
    }
    util::pause();
}

void SystemHub::printDateTime() {
    std::time_t now = std::time(nullptr);
    std::tm utc = util::safeGmTime(now);
    printKeyValue("Local time", util::timeToString(now, "%Y-%m-%d %H:%M:%S"));
    std::ostringstream utcStream;
    utcStream << std::put_time(&utc, "%Y-%m-%d %H:%M:%S");
    printKeyValue("UTC time", utcStream.str());
    printKeyValue("Local date", util::timeToString(now, "%A, %d %B %Y"));
}

void SystemHub::printSystemInfo() {
    printKeyValue("Current folder", std::filesystem::current_path().string());
    printKeyValue("Build stamp", std::string(__DATE__) + " " + __TIME__);
    printKeyValue("CPU threads", std::to_string(std::thread::hardware_concurrency()));
#ifdef _WIN32
    char computerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
    if (GetComputerNameA(computerName, &size)) {
        printKeyValue("Host", computerName);
    }
    MEMORYSTATUSEX status {};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        printKeyValue("Memory free", std::to_string(status.ullAvailPhys / (1024 * 1024)) + " MB");
    }
    printKeyValue("Platform", "Windows");
#else
    printKeyValue("Platform", "POSIX");
#endif
    auto space = std::filesystem::space(std::filesystem::current_path());
    printKeyValue("Disk free", std::to_string(space.available / (1024 * 1024)) + " MB");
}

void SystemHub::printWorkspaceSnapshot(const std::filesystem::path& root) {
    std::uintmax_t files = 0;
    std::uintmax_t folders = 0;
    std::uintmax_t bytes = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (entry.is_directory()) {
            ++folders;
        } else if (entry.is_regular_file()) {
            ++files;
            std::error_code error;
            bytes += entry.file_size(error);
        }
    }
    printKeyValue("Scan root", root.string());
    printKeyValue("Folders", std::to_string(folders));
    printKeyValue("Files", std::to_string(files));
    printKeyValue("Visible size", std::to_string(bytes / 1024) + " KB");
}

void MissionControl::printBrief(const User& user,
                                const TodoManager& todos,
                                const VaultManager& vault,
                                const HabitManager& habits,
                                const AlarmManager& alarms,
                                const MusicManager& music,
                                const VideoManager& videos) {
    std::cout << "Operator overview\n";
    std::cout << lineBar() << "\n";
    printKeyValue("Operator", user.username);
    printKeyValue("Motto", user.motto.empty() ? "-" : user.motto);
    printKeyValue("Timestamp", util::nowDateTime());
    std::cout << "\n";

    std::cout << "Applications status\n";
    std::cout << lineBar() << "\n";
    printKeyValue("Open tasks", std::to_string(todos.openCount()));
    printKeyValue("Completed tasks", std::to_string(todos.completedCount()));
    printKeyValue("Vault notes", std::to_string(vault.count()));
    printKeyValue("Habit streak", std::to_string(habits.strongestStreak()));
    printKeyValue("Live alarms", std::to_string(alarms.pendingCount()));
    printKeyValue("Music tracks", std::to_string(music.count()));
    printKeyValue("Video clips", std::to_string(videos.count()));
    std::cout << "\n";

    if (auto top = todos.topPriorityOpen()) {
        printKeyValue("Priority task", top->title + (top->dueDate.empty() ? "" : " | due " + top->dueDate));
    } else {
        printKeyValue("Priority task", "No open tasks.");
    }

    if (auto next = alarms.nextAlarm()) {
        printKeyValue("Next alarm", util::timeToString(next->trigger) + " | " + next->label);
    } else {
        printKeyValue("Next alarm", "No alarms scheduled.");
    }

    if (auto favorite = music.favorite()) {
        printKeyValue("Most played track", favorite->title + " | plays " + std::to_string(favorite->playCount));
    } else {
        printKeyValue("Most played track", "Music library is empty.");
    }

    if (auto latest = vault.latest()) {
        printKeyValue("Latest note", latest->title);
    } else {
        printKeyValue("Latest note", "Vault is empty.");
    }

    std::cout << "\nDirective\n";
    std::cout << lineBar() << "\n";
    std::cout << moodDirective(todos.openCount(), habits.strongestStreak()) << "\n";
}

MiniOS::MiniOS()
    : store_("data"),
      users_(store_),
      todos_(store_),
      vault_(store_),
      habits_(store_),
      alarms_(store_),
      music_(store_),
      videos_(store_),
      calculator_(store_),
      // Polymorphic module registry — AppModule* base pointers
      modules_({&todos_, &vault_, &habits_, &alarms_, &music_, &videos_}) {}

MiniOS::~MiniOS() {
    stopAlarmMonitor();
}

void MiniOS::run() {
    bootScreen();
    while (running_) {
        authLoop();
        if (currentUser_) {
            bindUserData();
            startAlarmMonitor();
            launcherLoop();
            stopAlarmMonitor();
            currentUser_.reset();
        }
    }
}

void MiniOS::bootScreen() const {
    printStandaloneTitle("Boot Sequence", "Launching your terminal workspace.");
    std::cout << "SMAAT is ready. Sign in to open applications.\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(900));
}

void MiniOS::authLoop() {
    while (running_ && !currentUser_) {
        showAuthHome();
        std::string choice = util::toLower(util::prompt("Select action: "));
        if (choice == "1" || choice == "login") {
            currentUser_ = users_.login();
        } else if (choice == "2" || choice == "register") {
            users_.registerUser();
        } else if (choice == "0" || choice == "exit" || choice == "shutdown") {
            running_ = false;
        } else {
            std::cout << "Choose one of the listed actions.\n";
            util::pause();
        }
    }
}

void MiniOS::launcherLoop() {
    while (running_ && currentUser_) {
        showLauncher();
        std::string choice = util::toLower(util::prompt("Open application: "));
        try {
            if (choice == "1") {
                missionControlApp();
            } else if (choice == "2") {
                notesSuite();
            } else if (choice == "3") {
                alarmCenter();
            } else if (choice == "4") {
                calculatorLab();
            } else if (choice == "5") {
                musicPlayer();
            } else if (choice == "6") {
                videoPlayer();
            } else if (choice == "7") {
                fileManager_.run();
            } else if (choice == "8") {
                systemCenter();
            } else if (choice == "9") {
                profileStudio();
            } else if (choice == "0" || choice == "logout") {
                currentUser_.reset();
            } else if (choice == "x" || choice == "exit" || choice == "shutdown") {
                currentUser_.reset();
                running_ = false;
            } else {
                std::cout << "Choose a launcher slot from the screen.\n";
                util::pause();
            }
        } catch (const std::exception& error) {
            std::cout << "SMAAT interaction error: " << error.what() << "\n";
            util::pause();
        }
    }
}

void MiniOS::showAuthHome() const {
    printStandaloneTitle("Identity Console", "Create a profile or sign in to the launcher.");
    printMenu({
        "1. Sign in",
        "2. Register new operator",
        "0. Shut down SMAAT"
    });
}

void MiniOS::showLauncher() const {
    printAppTitle("Application Launcher", currentUser_, "Choose a workspace instead of typing commands.");
    printKeyValue("Open tasks", std::to_string(todos_.openCount()));
    printKeyValue("Vault notes", std::to_string(vault_.count()));
    printKeyValue("Habits", std::to_string(habits_.count()));
    printKeyValue("Alarms", std::to_string(alarms_.pendingCount()));
    printKeyValue("Music", std::to_string(music_.count()));
    printKeyValue("Videos", std::to_string(videos_.count()));
    if (auto next = alarms_.nextAlarm()) {
        printKeyValue("Next alarm", util::timeToString(next->trigger) + " | " + next->label);
    }
    std::cout << "\nApplications\n";
    std::cout << lineBar() << "\n";
    printMenu({
        "1. Mission Control",
        "2. Notes Suite",
        "3. Alarm Center",
        "4. Calculator Lab",
        "5. Music Player",
        "6. Video Player",
        "7. File Manager",
        "8. System Center",
        "9. Profile Studio",
        "0. Sign out",
        "X. Shut down SMAAT"
    });
}

void MiniOS::showSupportPage(const std::string& title, const std::vector<std::string>& lines) const {
    printAppTitle(title, currentUser_, "Use this page when you need a quick reminder.");
    printMenu(lines);
    util::pause();
}

void MiniOS::profileStudio() {
    while (currentUser_) {
        printAppTitle("Profile Studio", currentUser_, "Tune identity settings for this operator.");
        printKeyValue("Operator", currentUser_->username);
        printKeyValue("Created", currentUser_->createdAt);
        printKeyValue("Motto", currentUser_->motto.empty() ? "-" : currentUser_->motto);
        std::cout << "\n";
        printMenu({
            "1. Edit motto",
            "2. Change access key",
            "3. Support",
            "0. Back to launcher"
        });

        std::string choice = util::prompt("Choose action: ");
        if (choice == "1") {
            currentUser_->motto = util::prompt("New motto: ");
            users_.updateUser(*currentUser_);
            std::cout << "Motto updated.\n";
            util::pause();
        } else if (choice == "2") {
            users_.changePassword(*currentUser_);
        } else if (choice == "3") {
            showSupportPage("Profile Studio Support", {
                "Edit motto: update the sentence shown on the launcher and Mission Control.",
                "Change access key: rotate the password used to sign in to this operator profile.",
                "Operator data is stored per user in the local data folder."
            });
        } else if (choice == "0") {
            return;
        } else {
            std::cout << "Choose one of the listed actions.\n";
            util::pause();
        }
    }
}

void MiniOS::missionControlApp() const {
    while (currentUser_) {
        printAppTitle("Mission Control", currentUser_, "High-level view of your system and work state.");
        MissionControl::printBrief(*currentUser_, todos_, vault_, habits_, alarms_, music_, videos_);
        std::cout << "\n";
        printMenu({
            "1. Refresh briefing",
            "2. Support",
            "0. Back to launcher"
        });
        std::string choice = util::prompt("Choose action: ");
        if (choice == "1") {
            continue;
        }
        if (choice == "2") {
            showSupportPage("Mission Control Support", {
                "Mission Control summarizes every major application in SMAAT.",
                "Open tasks come from the Notes Suite task board.",
                "Latest note comes from the vault.",
                "Next alarm comes from Alarm Center.",
                "Most played track comes from Music Player."
            });
            continue;
        }
        if (choice == "0") {
            return;
        }
    }
}

void MiniOS::notesSuite() {
    while (currentUser_) {
        printAppTitle("Notes Suite", currentUser_, "Tasks, vault entries, and habits live together here.");
        printKeyValue("Open tasks", std::to_string(todos_.openCount()));
        printKeyValue("Completed tasks", std::to_string(todos_.completedCount()));
        printKeyValue("Vault entries", std::to_string(vault_.count()));
        printKeyValue("Strongest streak", std::to_string(habits_.strongestStreak()));
        if (auto top = todos_.topPriorityOpen()) {
            printKeyValue("Top task", top->title);
        }
        if (auto latest = vault_.latest()) {
            printKeyValue("Latest note", latest->title);
        }
        std::cout << "\n";
        printMenu({
            "1. Review board",
            "2. Add task",
            "3. Complete task",
            "4. Delete task",
            "5. Add vault note",
            "6. Read vault note",
            "7. Search vault",
            "8. Delete vault note",
            "9. Habit board",
            "10. Support",
            "0. Back to launcher"
        });

        std::string choice = util::prompt("Choose action: ");
        if (choice == "1") {
            printAppTitle("Notes Suite Review", currentUser_, "Tasks, notes, and habits at a glance.");
            std::cout << "Tasks\n" << lineBar() << "\n";
            todos_.list();
            std::cout << "\nVault\n" << lineBar() << "\n";
            vault_.list();
            std::cout << "\nHabits\n" << lineBar() << "\n";
            habits_.list();
            util::pause();
        } else if (choice == "2") {
            todos_.addInteractive();
            util::pause();
        } else if (choice == "3") {
            if (!todos_.markDone(promptId("Task ID to complete: "))) {
                std::cout << "Task not found.\n";
            }
            util::pause();
        } else if (choice == "4") {
            if (!todos_.remove(promptId("Task ID to delete: "))) {
                std::cout << "Task not found.\n";
            }
            util::pause();
        } else if (choice == "5") {
            vault_.addInteractive();
            util::pause();
        } else if (choice == "6") {
            if (!vault_.read(promptId("Vault note ID to open: "))) {
                std::cout << "Note not found.\n";
            }
            util::pause();
        } else if (choice == "7") {
            vault_.search(util::prompt("Search text: "));
            util::pause();
        } else if (choice == "8") {
            if (!vault_.remove(promptId("Vault note ID to delete: "))) {
                std::cout << "Note not found.\n";
            }
            util::pause();
        } else if (choice == "9") {
            habitBoard();
        } else if (choice == "10") {
            showSupportPage("Notes Suite Support", {
                "Add task: creates a tracked to-do with category, due date, and priority.",
                "Complete task: marks a task done without deleting the record.",
                "Vault note: stores longer writing, ideas, and references.",
                "Search vault: finds notes by title, body, or tags.",
                "Habit board: track repeat behavior with streaks and check-ins."
            });
        } else if (choice == "0") {
            return;
        } else {
            std::cout << "Choose one of the listed actions.\n";
            util::pause();
        }
    }
}

void MiniOS::habitBoard() {
    while (currentUser_) {
        printAppTitle("Habit Board", currentUser_, "Keep streaks alive and measure consistency.");
        habits_.list();
        std::cout << "\n";
        printMenu({
            "1. Add habit",
            "2. Check in",
            "3. Delete habit",
            "4. Support",
            "0. Back to Notes Suite"
        });

        std::string choice = util::prompt("Choose action: ");
        if (choice == "1") {
            habits_.addInteractive();
            util::pause();
        } else if (choice == "2") {
            if (!habits_.checkIn(promptId("Habit ID to check in: "))) {
                std::cout << "Habit not found.\n";
            }
            util::pause();
        } else if (choice == "3") {
            if (!habits_.remove(promptId("Habit ID to delete: "))) {
                std::cout << "Habit not found.\n";
            }
            util::pause();
        } else if (choice == "4") {
            showSupportPage("Habit Board Support", {
                "Add habit: create a daily or weekly behavior you want to track.",
                "Check in: marks today complete for that habit and updates the streak.",
                "Delete habit: removes the habit and its saved streak data."
            });
        } else if (choice == "0") {
            return;
        } else {
            std::cout << "Choose one of the listed actions.\n";
            util::pause();
        }
    }
}

void MiniOS::alarmCenter() {
    while (currentUser_) {
        printAppTitle("Alarm Center", currentUser_, "Schedule reminders that trigger when SMAAT is open and signed in.");
        printKeyValue("Pending alarms", std::to_string(alarms_.pendingCount()));
        if (auto next = alarms_.nextAlarm()) {
            printKeyValue("Next alarm", util::timeToString(next->trigger) + " | " + next->label);
        }
        std::cout << "\n";
        alarms_.list();
        std::cout << "\n";
        printMenu({
            "1. Add alarm",
            "2. Delete alarm",
            "3. Support",
            "0. Back to launcher"
        });

        std::string choice = util::prompt("Choose action: ");
        if (choice == "1") {
            alarms_.addInteractive();
            util::pause();
        } else if (choice == "2") {
            if (!alarms_.remove(promptId("Alarm ID to delete: "))) {
                std::cout << "Alarm not found.\n";
            }
            util::pause();
        } else if (choice == "3") {
            showSupportPage("Alarm Center Support", {
                "Add alarm: enter a label and a time using YYYY-MM-DD HH:MM.",
                "When an alarm time arrives and you are signed in, SMAAT prints an alert block in the terminal.",
                "If the app is reopened after the alarm time, the pending alert will appear almost immediately after sign-in.",
                "Delete alarm: removes a saved reminder from your schedule."
            });
        } else if (choice == "0") {
            return;
        } else {
            std::cout << "Choose one of the listed actions.\n";
            util::pause();
        }
    }
}

void MiniOS::calculatorLab() {
    while (currentUser_) {
        printAppTitle("Calculator Lab", currentUser_, "Evaluate expressions, use functions, and inspect history.");
        printMenu({
            "1. Solve one expression",
            "2. Open expression mode",
            "3. Show history",
            "4. Support",
            "0. Back to launcher"
        });

        std::string choice = util::prompt("Choose action: ");
        if (choice == "1") {
            calculator_.evaluateAndPrint(util::prompt("Expression: "));
            util::pause();
        } else if (choice == "2") {
            printAppTitle("Calculator Lab", currentUser_, "Expression mode");
            calculator_.repl();
        } else if (choice == "3") {
            printAppTitle("Calculator History", currentUser_, "Recent results stored for this operator.");
            calculator_.showHistory();
            util::pause();
        } else if (choice == "4") {
            showSupportPage("Calculator Lab Support", {
                "You can solve expressions like 2 + 2 * 5 or (3^2) + sqrt(16).",
                "Supported functions: sqrt, sin, cos, tan, log, abs.",
                "Use ans to reuse the last answer inside the same session.",
                "Expression mode keeps asking for expressions until you submit a blank line."
            });
        } else if (choice == "0") {
            return;
        } else {
            std::cout << "Choose one of the listed actions.\n";
            util::pause();
        }
    }
}

void MiniOS::musicPlayer() {
    while (currentUser_) {
        printAppTitle("Music Player", currentUser_, "Maintain a track library and open songs with your system player.");
        printKeyValue("Tracks", std::to_string(music_.count()));
        if (auto favorite = music_.favorite()) {
            printKeyValue("Most played", favorite->title + " | " + std::to_string(favorite->playCount) + " plays");
        }
        std::cout << "\n";
        music_.list();
        std::cout << "\n";
        printMenu({
            "1. Add track",
            "2. Play track",
            "3. Shuffle play",
            "4. Delete track",
            "5. Support",
            "0. Back to launcher"
        });

        std::string choice = util::prompt("Choose action: ");
        if (choice == "1") {
            music_.addInteractive();
            util::pause();
        } else if (choice == "2") {
            if (!music_.play(promptId("Track ID to play: "))) {
                std::cout << "Track not found or could not be opened.\n";
            }
            util::pause();
        } else if (choice == "3") {
            if (!music_.shufflePlay()) {
                std::cout << "No tracks were available for shuffle play.\n";
            }
            util::pause();
        } else if (choice == "4") {
            if (!music_.remove(promptId("Track ID to delete: "))) {
                std::cout << "Track not found.\n";
            }
            util::pause();
        } else if (choice == "5") {
            showSupportPage("Music Player Support", {
                "Add track: store a title, artist, mood, and full path to a real audio file.",
                "Play track: opens the chosen file using your default operating system media player.",
                "Shuffle play: randomly picks one saved track and opens it.",
                "Delete track: removes the saved library entry, not the actual audio file on disk."
            });
        } else if (choice == "0") {
            return;
        } else {
            std::cout << "Choose one of the listed actions.\n";
            util::pause();
        }
    }
}

void MiniOS::videoPlayer() {
    while (currentUser_) {
        printAppTitle("Video Player", currentUser_, "Keep a launchable library of local video files.");
        printKeyValue("Videos", std::to_string(videos_.count()));
        std::cout << "\n";
        videos_.list();
        std::cout << "\n";
        printMenu({
            "1. Add video",
            "2. Open video",
            "3. Delete video",
            "4. Support",
            "0. Back to launcher"
        });

        std::string choice = util::prompt("Choose action: ");
        if (choice == "1") {
            videos_.addInteractive();
            util::pause();
        } else if (choice == "2") {
            if (!videos_.play(promptId("Video ID to open: "))) {
                std::cout << "Video not found or could not be opened.\n";
            }
            util::pause();
        } else if (choice == "3") {
            if (!videos_.remove(promptId("Video ID to delete: "))) {
                std::cout << "Video not found.\n";
            }
            util::pause();
        } else if (choice == "4") {
            showSupportPage("Video Player Support", {
                "Add video: save a title, category, and full path to a local video file.",
                "Open video: launches the chosen file using the operating system default video player.",
                "Delete video: removes the library record, not the actual video file on disk."
            });
        } else if (choice == "0") {
            return;
        } else {
            std::cout << "Choose one of the listed actions.\n";
            util::pause();
        }
    }
}

void MiniOS::systemCenter() const {
    while (currentUser_) {
        printAppTitle("System Center", currentUser_, "Time, calendar, machine details, and workspace scan tools.");
        printMenu({
            "1. Show live date and time",
            "2. Show month calendar",
            "3. Show system report",
            "4. Show workspace snapshot",
            "5. Module status (all apps)",
            "6. Support",
            "0. Back to launcher"
        });

        std::string choice = util::prompt("Choose action: ");
        if (choice == "1") {
            printAppTitle("System Center", currentUser_, "Current time");
            SystemHub::printDateTime();
            util::pause();
        } else if (choice == "2") {
            std::time_t now = std::time(nullptr);
            std::tm local = util::safeLocalTime(now);
            std::string monthInput = util::prompt("Month 1-12 (blank = current): ");
            std::string yearInput = util::prompt("Year (blank = current): ");
            int month = monthInput.empty() ? (local.tm_mon + 1) : parseWholeNumber(monthInput);
            int year = yearInput.empty() ? (local.tm_year + 1900) : parseWholeNumber(yearInput);
            if (month < 1 || month > 12) {
                throw std::runtime_error("Month must be between 1 and 12.");
            }
            printAppTitle("System Center", currentUser_, "Month calendar");
            util::printMonthCalendar(year, month);
            util::pause();
        } else if (choice == "3") {
            printAppTitle("System Center", currentUser_, "System report");
            SystemHub::printSystemInfo();
            util::pause();
        } else if (choice == "4") {
            printAppTitle("System Center", currentUser_, "Workspace snapshot");
            SystemHub::printWorkspaceSnapshot(std::filesystem::current_path());
            util::pause();
        } else if (choice == "5") {
            // Polymorphism in action: iterate AppModule* base pointers,
            // calling the overridden virtual printSummary() / count() / moduleName()
            printAppTitle("System Center", currentUser_, "Module status");
            std::cout << "Active application modules:\n";
            std::cout << lineBar() << "\n";
            for (AppModule* module : modules_) {
                module->printSummary();   // virtual dispatch
            }
            util::pause();
        } else if (choice == "6") {
            showSupportPage("System Center Support", {
                "Show live date and time: prints local and UTC timestamps.",
                "Show month calendar: render any month and year directly inside the terminal.",
                "Show system report: machine-oriented facts like disk, memory, platform, and current folder.",
                "Show workspace snapshot: counts files and folders below the current working directory.",
                "Module status: prints item counts for every registered app module via polymorphism."
            });
        } else if (choice == "0") {
            return;
        } else {
            std::cout << "Choose one of the listed actions.\n";
            util::pause();
        }
    }
}

void MiniOS::bindUserData() {
    const std::string username = currentUser_->username;
    todos_.bindUser(username);
    vault_.bindUser(username);
    habits_.bindUser(username);
    alarms_.bindUser(username);
    music_.bindUser(username);
    videos_.bindUser(username);
    calculator_.bindUser(username);
}

void MiniOS::startAlarmMonitor() {
    monitorRunning_ = true;
    monitorThread_ = std::thread([this]() {
        while (monitorRunning_) {
            auto due = alarms_.collectDue();
            if (!due.empty()) {
                std::lock_guard<std::mutex> lock(outputMutex_);
                std::cout << "\n";
                std::cout << lineBar() << "\n";
                std::cout << "SMAAT Alarm Center Alert\n";
                std::cout << lineBar() << "\n";
                for (const auto& alarm : due) {
                    std::cout << "Alarm fired: " << alarm.label
                              << " | " << util::timeToString(alarm.trigger) << "\n";
                }
                std::cout << lineBar() << "\n";
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
}

void MiniOS::stopAlarmMonitor() {
    monitorRunning_ = false;
    if (monitorThread_.joinable()) {
        monitorThread_.join();
    }
}

int MiniOS::parseWholeNumber(const std::string& value) const {
    std::stringstream stream(value);
    int number {};
    char extra {};
    if (!(stream >> number) || (stream >> extra)) {
        throw std::runtime_error("Expected a whole number but got: " + value);
    }
    return number;
}

int MiniOS::promptId(const std::string& label) const {
    return parseWholeNumber(util::prompt(label));
}
