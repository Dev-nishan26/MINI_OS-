#pragma once

#include "Utilities.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// ── Operator overloading & friend functions ──────────────────────────────────
// Each model struct overloads operator<< for display and operator== for
// equality comparison.  The stream operators are declared as friend functions
// so they can access private/protected data if needed and satisfy the
// "friend function" requirement.
// ─────────────────────────────────────────────────────────────────────────────

struct User {
    std::string username;
    std::string salt;
    std::string passwordHash;
    std::string motto;
    std::string createdAt;

    // Operator overloading: equality based on username
    bool operator==(const User& other) const {
        return username == other.username;
    }
    bool operator!=(const User& other) const {
        return !(*this == other);
    }

    // Friend function: stream output for User
    friend std::ostream& operator<<(std::ostream& os, const User& u) {
        os << "User[" << u.username << "]";
        if (!u.motto.empty()) os << " motto=\"" << u.motto << "\"";
        return os;
    }

    std::string serialize() const {
        return util::join({
            util::escapeField(username),
            util::escapeField(salt),
            util::escapeField(passwordHash),
            util::escapeField(motto),
            util::escapeField(createdAt)
        }, "|");
    }

    static User deserialize(const std::string& line) {
        auto fields = util::parseFields(line);
        if (fields.size() < 5) {
            throw std::runtime_error("Malformed user record");
        }
        return {
            util::unescapeField(fields[0]),
            util::unescapeField(fields[1]),
            util::unescapeField(fields[2]),
            util::unescapeField(fields[3]),
            util::unescapeField(fields[4])
        };
    }
};

struct Todo {
    int id {};
    std::string title;
    std::string category;
    std::string dueDate;
    int priority {};
    bool done {};
    std::string createdAt;

    // Operator overloading: compare by priority (higher = greater)
    bool operator>(const Todo& other) const { return priority > other.priority; }
    bool operator<(const Todo& other) const { return priority < other.priority; }
    bool operator==(const Todo& other) const { return id == other.id; }

    // Friend function: stream output for Todo
    friend std::ostream& operator<<(std::ostream& os, const Todo& t) {
        os << "#" << t.id << " [" << (t.done ? "done" : "open") << "] P"
           << t.priority << " " << t.title;
        if (!t.dueDate.empty()) os << " (due: " << t.dueDate << ")";
        return os;
    }

    std::string serialize() const {
        return util::join({
            std::to_string(id),
            util::escapeField(title),
            util::escapeField(category),
            util::escapeField(dueDate),
            std::to_string(priority),
            done ? "1" : "0",
            util::escapeField(createdAt)
        }, "|");
    }

    static Todo deserialize(const std::string& line) {
        auto fields = util::parseFields(line);
        if (fields.size() < 7) {
            throw std::runtime_error("Malformed todo record");
        }
        return {
            std::stoi(fields[0]),
            util::unescapeField(fields[1]),
            util::unescapeField(fields[2]),
            util::unescapeField(fields[3]),
            std::stoi(fields[4]),
            fields[5] == "1",
            util::unescapeField(fields[6])
        };
    }
};

struct Note {
    int id {};
    std::string title;
    std::string body;
    std::string tags;
    std::string createdAt;

    bool operator==(const Note& other) const { return id == other.id; }

    // Friend function: stream output for Note
    friend std::ostream& operator<<(std::ostream& os, const Note& n) {
        os << "#" << n.id << " \"" << n.title << "\"";
        if (!n.tags.empty()) os << " [" << n.tags << "]";
        return os;
    }

    std::string serialize() const {
        return util::join({
            std::to_string(id),
            util::escapeField(title),
            util::escapeField(body),
            util::escapeField(tags),
            util::escapeField(createdAt)
        }, "|");
    }

    static Note deserialize(const std::string& line) {
        auto fields = util::parseFields(line);
        if (fields.size() < 5) {
            throw std::runtime_error("Malformed note record");
        }
        return {
            std::stoi(fields[0]),
            util::unescapeField(fields[1]),
            util::unescapeField(fields[2]),
            util::unescapeField(fields[3]),
            util::unescapeField(fields[4])
        };
    }
};

struct Alarm {
    int id {};
    std::string label;
    std::time_t trigger {};
    bool fired {};
    std::string createdAt;

    std::string serialize() const {
        return util::join({
            std::to_string(id),
            util::escapeField(label),
            std::to_string(static_cast<long long>(trigger)),
            fired ? "1" : "0",
            util::escapeField(createdAt)
        }, "|");
    }

    static Alarm deserialize(const std::string& line) {
        auto fields = util::parseFields(line);
        if (fields.size() < 5) {
            throw std::runtime_error("Malformed alarm record");
        }
        return {
            std::stoi(fields[0]),
            util::unescapeField(fields[1]),
            static_cast<std::time_t>(std::stoll(fields[2])),
            fields[3] == "1",
            util::unescapeField(fields[4])
        };
    }
};

struct Song {
    int id {};
    std::string title;
    std::string artist;
    std::string path;
    std::string mood;
    int playCount {};
    std::string addedAt;

    bool operator==(const Song& other) const { return id == other.id; }
    // Operator overloading: compare songs by play count
    bool operator<(const Song& other) const { return playCount < other.playCount; }
    bool operator>(const Song& other) const { return playCount > other.playCount; }

    // Friend function: stream output for Song
    friend std::ostream& operator<<(std::ostream& os, const Song& s) {
        os << "#" << s.id << " \"" << s.title << "\" by "
           << (s.artist.empty() ? "Unknown" : s.artist)
           << " (" << s.playCount << " plays)";
        return os;
    }

    std::string serialize() const {
        return util::join({
            std::to_string(id),
            util::escapeField(title),
            util::escapeField(artist),
            util::escapeField(path),
            util::escapeField(mood),
            std::to_string(playCount),
            util::escapeField(addedAt)
        }, "|");
    }

    static Song deserialize(const std::string& line) {
        auto fields = util::parseFields(line);
        if (fields.size() < 7) {
            throw std::runtime_error("Malformed song record");
        }
        return {
            std::stoi(fields[0]),
            util::unescapeField(fields[1]),
            util::unescapeField(fields[2]),
            util::unescapeField(fields[3]),
            util::unescapeField(fields[4]),
            std::stoi(fields[5]),
            util::unescapeField(fields[6])
        };
    }
};

struct VideoClip {
    int id {};
    std::string title;
    std::string category;
    std::string path;
    int watchCount {};
    std::string addedAt;

    std::string serialize() const {
        return util::join({
            std::to_string(id),
            util::escapeField(title),
            util::escapeField(category),
            util::escapeField(path),
            std::to_string(watchCount),
            util::escapeField(addedAt)
        }, "|");
    }

    static VideoClip deserialize(const std::string& line) {
        auto fields = util::parseFields(line);
        if (fields.size() < 6) {
            throw std::runtime_error("Malformed video record");
        }
        return {
            std::stoi(fields[0]),
            util::unescapeField(fields[1]),
            util::unescapeField(fields[2]),
            util::unescapeField(fields[3]),
            std::stoi(fields[4]),
            util::unescapeField(fields[5])
        };
    }
};

struct Habit {
    int id {};
    std::string name;
    int streak {};
    std::string lastCheckin;
    int totalCheckins {};
    std::string cadence;

    std::string serialize() const {
        return util::join({
            std::to_string(id),
            util::escapeField(name),
            std::to_string(streak),
            util::escapeField(lastCheckin),
            std::to_string(totalCheckins),
            util::escapeField(cadence)
        }, "|");
    }

    static Habit deserialize(const std::string& line) {
        auto fields = util::parseFields(line);
        if (fields.size() < 6) {
            throw std::runtime_error("Malformed habit record");
        }
        return {
            std::stoi(fields[0]),
            util::unescapeField(fields[1]),
            std::stoi(fields[2]),
            util::unescapeField(fields[3]),
            std::stoi(fields[4]),
            util::unescapeField(fields[5])
        };
    }
};

struct CalcEntry {
    int id {};
    std::string expression;
    double result {};
    std::string createdAt;

    std::string serialize() const {
        return util::join({
            std::to_string(id),
            util::escapeField(expression),
            util::escapeField(std::to_string(result)),
            util::escapeField(createdAt)
        }, "|");
    }

    static CalcEntry deserialize(const std::string& line) {
        auto fields = util::parseFields(line);
        if (fields.size() < 4) {
            throw std::runtime_error("Malformed calculator record");
        }
        return {
            std::stoi(fields[0]),
            util::unescapeField(fields[1]),
            std::stod(util::unescapeField(fields[2])),
            util::unescapeField(fields[3])
        };
    }
};
