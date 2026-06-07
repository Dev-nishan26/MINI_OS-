#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

class DataStore {
public:
    explicit DataStore(std::filesystem::path root = "data") : root_(std::move(root)) {
        ensure();
    }

    void ensure() const {
        std::filesystem::create_directories(root_);
    }

    std::filesystem::path resolve(const std::string& fileName) const {
        return root_ / fileName;
    }

    std::vector<std::string> loadLines(const std::string& fileName) const {
        std::vector<std::string> lines;
        std::ifstream input(resolve(fileName));
        std::string line;
        while (std::getline(input, line)) {
            lines.push_back(line);
        }
        return lines;
    }

    void saveLines(const std::string& fileName, const std::vector<std::string>& lines) const {
        ensure();
        std::ofstream output(resolve(fileName), std::ios::trunc);
        for (const auto& line : lines) {
            output << line << '\n';
        }
    }

    template <typename T>
    std::vector<T> loadList(const std::string& fileName) const {
        std::vector<T> records;
        for (const auto& line : loadLines(fileName)) {
            if (line.empty()) {
                continue;
            }
            try {
                records.push_back(T::deserialize(line));
            } catch (...) {
                
            }
        }
        return records;
    }

    template <typename T>
    void saveList(const std::string& fileName, const std::vector<T>& records) const {
        std::vector<std::string> lines;
        lines.reserve(records.size());
        for (const auto& record : records) {
            lines.push_back(record.serialize());
        }
        saveLines(fileName, lines);
    }

private:
    std::filesystem::path root_;
};

template <typename T>
class Repository {
public:
    Repository(const DataStore& store, std::string fileName = {})
        : store_(&store), fileName_(std::move(fileName)) {
        if (!fileName_.empty()) {
            load();
        }
    }

    void useFile(std::string fileName) {
        fileName_ = std::move(fileName);
        load();
    }

    void load() {
        if (!fileName_.empty()) {
            items_ = store_->loadList<T>(fileName_);
        }
    }

    void save() const {
        if (!fileName_.empty()) {
            store_->saveList(fileName_, items_);
        }
    }

    std::vector<T>& items() {
        return items_;
    }

    const std::vector<T>& items() const {
        return items_;
    }

    int nextId() const {
        int highest = 0;
        for (const auto& item : items_) {
            if (item.id > highest) {
                highest = item.id;
            }
        }
        return highest + 1;
    }

private:
    const DataStore* store_ {};
    std::string fileName_;
    std::vector<T> items_;
};
