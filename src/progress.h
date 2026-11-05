// progress.h - persistent learner history: sessions, per-question stats, coding attempts, settings
#pragma once
#include "common.h"
#include <map>

struct ItemStat {
    int attempts = 0;
    int correct = 0;
    long long last = 0;
    bool lastCorrect = false;
};

struct Session {
    long long time = 0;
    std::string mode;      // "mcq", "written", "coding", "mock", "drill", "reading"
    std::string topic;     // topic id, "exam1", "exam2", "mixed", set id ...
    std::string title;     // human readable
    int score = 0;
    int total = 0;
    float seconds = 0;
    std::vector<std::pair<std::string, bool>> items;
};

struct CodeStat {
    int attempts = 0;
    int bestPassed = 0;
    int total = 0;
    bool solved = false;
    long long last = 0;
};

struct Settings {
    float volume = 0.8f;
    bool voice = true;
    bool quickFetch = false;
    bool fullscreen = false;
    bool mouseLook = true;
};

struct TopicMastery {
    std::string topic;
    int attempts = 0;
    int correct = 0;
    int codeSolved = 0;
    int codeTotal = 0;
    float score = -1.f;    // 0..1, -1 if not yet assessed
};

class Progress {
public:
    std::vector<Session> sessions;
    std::map<std::string, ItemStat> items;
    std::map<std::string, CodeStat> code;
    std::map<std::string, int> chaptersRead;   // topic -> furthest page reached
    Settings settings;
    int booksReturned = 0;

    void Load();
    void Save();
    void Reset();

    void RecordSession(const Session& s);
    void RecordAnswer(const std::string& id, bool correct);
    void RecordCode(const std::string& id, int passed, int total);
    ItemStat Item(const std::string& id) const;
    CodeStat Code(const std::string& id) const;

    std::string LoadDraft(const std::string& problemId) const;
    void SaveDraft(const std::string& problemId, const std::string& text) const;

    std::vector<TopicMastery> Mastery() const;   // one entry per topic (content order)
    int TotalAnswered() const;
    float OverallAccuracy() const;
};

Progress& Prog();
