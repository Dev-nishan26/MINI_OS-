#pragma once

#include "DataStore.h"
#include "Models.h"

#include <atomic>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// ── Abstract base class (pure virtual / polymorphism) ─────────────────────
// AppModule is an abstract interface that every manager must implement.
// This satisfies the inheritance + polymorphism OOP requirement.
// MiniOS can hold a vector<AppModule*> to iterate over all modules uniformly.
// ─────────────────────────────────────────────────────────────────────────────
class AppModule {
public:
    virtual ~AppModule() = default;

    // Pure virtual functions — subclasses must override
    virtual void list() const = 0;
    virtual int  count() const = 0;
    virtual std::string moduleName() const = 0;

    // Non-virtual helper: print a one-line summary (uses virtual count())
    void printSummary() const {
        std::cout << "[" << moduleName() << "] items: " << count() << "\n";
    }
};
// ─────────────────────────────────────────────────────────────────────────────

class UserManager {
public:
    explicit UserManager(DataStore& store);

    bool registerUser();
    std::optional<User> login();
    void updateUser(const User& user);
    bool changePassword(User& user);

private:
    void load();
    void save() const;
    bool usernameExists(const std::string& username) const;

    DataStore& store_;
    std::vector<User> users_;
};

class TodoManager : public AppModule {
public:
    explicit TodoManager(const DataStore& store);
    void bindUser(const std::string& username);
    void addInteractive();
    void list() const override;
    bool markDone(int id);
    bool remove(int id);
    int openCount() const;
    int completedCount() const;
    int count() const override;
    std::string moduleName() const override { return "TodoManager"; }
    const std::vector<Todo>& items() const;
    std::optional<Todo> topPriorityOpen() const;

private:
    Repository<Todo> repo_;
};

class VaultManager : public AppModule {
public:
    explicit VaultManager(const DataStore& store);
    void bindUser(const std::string& username);
    void addInteractive();
    void list() const override;
    bool read(int id) const;
    bool remove(int id);
    void search(const std::string& query) const;
    int count() const override;
    std::string moduleName() const override { return "VaultManager"; }
    std::optional<Note> latest() const;

private:
    Repository<Note> repo_;
};

class HabitManager : public AppModule {
public:
    explicit HabitManager(const DataStore& store);
    void bindUser(const std::string& username);
    void addInteractive();
    void list() const override;
    bool checkIn(int id);
    bool remove(int id);
    int strongestStreak() const;
    int count() const override;
    std::string moduleName() const override { return "HabitManager"; }

private:
    Repository<Habit> repo_;
};

class AlarmManager : public AppModule {
public:
    explicit AlarmManager(const DataStore& store);
    void bindUser(const std::string& username);
    void addInteractive();
    void list() const override;
    bool remove(int id);
    std::vector<Alarm> collectDue();
    int pendingCount() const;
    int count() const override;
    std::string moduleName() const override { return "AlarmManager"; }
    std::optional<Alarm> nextAlarm() const;

private:
    mutable std::mutex mutex_;
    Repository<Alarm> repo_;
};

class MusicManager : public AppModule {
public:
    explicit MusicManager(const DataStore& store);
    void bindUser(const std::string& username);
    void addInteractive();
    void list() const override;
    bool play(int id);
    bool shufflePlay();
    bool remove(int id);
    int count() const override;
    std::string moduleName() const override { return "MusicManager"; }
    std::optional<Song> favorite() const;

private:
    Repository<Song> repo_;
};

class VideoManager : public AppModule {
public:
    explicit VideoManager(const DataStore& store);
    void bindUser(const std::string& username);
    void addInteractive();
    void list() const override;
    bool play(int id);
    bool remove(int id);
    int count() const override;
    std::string moduleName() const override { return "VideoManager"; }

private:
    Repository<VideoClip> repo_;
};

class CalculatorManager {
public:
    explicit CalculatorManager(const DataStore& store);
    void bindUser(const std::string& username);
    void repl();
    bool evaluateAndPrint(const std::string& expression);
    void showHistory() const;

private:
    double lastAnswer_ {};
    Repository<CalcEntry> repo_;
};

class FileManagerApp {
public:
    FileManagerApp();
    void run();

private:
    std::filesystem::path resolve(const std::string& input) const;
    void showDirectory() const;
    void showSupport() const;
    void openFolder();
    void goUp();
    void previewFile() const;
    void createFolder() const;
    void createFile() const;
    void copyItem() const;
    void moveItem() const;
    void removeItem() const;
    void searchItem() const;

    std::filesystem::path currentPath_;
};

class SystemHub {
public:
    static void printDateTime();
    static void printSystemInfo();
    static void printWorkspaceSnapshot(const std::filesystem::path& root);
};

class MissionControl {
public:
    static void printBrief(const User& user,
                           const TodoManager& todos,
                           const VaultManager& vault,
                           const HabitManager& habits,
                           const AlarmManager& alarms,
                           const MusicManager& music,
                           const VideoManager& videos);
};

class MiniOS {
public:
    MiniOS();
    ~MiniOS();

    void run();

private:
    void bootScreen() const;
    void authLoop();
    void launcherLoop();
    void showAuthHome() const;
    void showLauncher() const;
    void showSupportPage(const std::string& title, const std::vector<std::string>& lines) const;
    void profileStudio();
    void missionControlApp() const;
    void notesSuite();
    void habitBoard();
    void alarmCenter();
    void calculatorLab();
    void musicPlayer();
    void videoPlayer();
    void systemCenter() const;
    void bindUserData();
    void startAlarmMonitor();
    void stopAlarmMonitor();
    int parseWholeNumber(const std::string& value) const;
    int promptId(const std::string& label) const;

    DataStore store_;
    UserManager users_;
    TodoManager todos_;
    VaultManager vault_;
    HabitManager habits_;
    AlarmManager alarms_;
    MusicManager music_;
    VideoManager videos_;
    CalculatorManager calculator_;
    FileManagerApp fileManager_;
    std::optional<User> currentUser_;
    std::atomic<bool> running_ {true};
    std::atomic<bool> monitorRunning_ {false};
    std::thread monitorThread_;
    mutable std::mutex outputMutex_;
    // Polymorphic list of all AppModule subclasses for uniform operations
    std::vector<AppModule*> modules_;
};
