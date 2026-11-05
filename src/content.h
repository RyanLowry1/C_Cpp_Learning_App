// content.h - learning content model (topics, questions, chapters, coding problems)
// All content lives in plain-text files under assets/content so it can be edited
// or extended without recompiling (see assets/content/README.txt for the format).
#pragma once
#include "common.h"
#include <map>

struct Topic {
    std::string id;
    std::string name;       // "Pointers"
    std::string blurb;      // one-line description
    std::string emblem;     // short symbol printed on the cover, e.g. "*p"
    Color color;            // leather colour of the book
    int exam;               // 1 = Coding Exam 1 material, 2 = Coding Exam 2, 3 = C++ extras
    int bay;                // shelf bay (0..5)
    int side;               // 0 = left half of the bay, 1 = right half
};

struct MCQ {
    std::string id, topic, question, code, why;
    std::vector<std::string> options;
    int correct = 0;
    int diff = 1;
    bool verifyOutput = false;   // self-test: correct option must equal program output
};

struct Written {
    std::string id, topic, kind, question, code, why;
    std::vector<std::string> answers;
    bool runnable = false;       // self-test: code is a complete program whose output == answers[0]
    bool multiline = false;
};

struct MemCell { std::string name, type, value, addr, pointsTo; };

struct ReadBlock {
    enum Type { Heading, Sub, Para, Code, Bullet, Tip, Warn, Exam, Mem, Break, Output };
    Type type;
    std::string text;
    std::vector<MemCell> mem;
};

struct Chapter {
    std::string topic, title, subtitle;
    std::vector<ReadBlock> blocks;
};

struct TestCase {
    std::string label;
    std::string code;
    bool sample = false;
};

struct ForbidRule {
    std::vector<std::string> tokens;
    std::string message;
};

struct Problem {
    std::string id, set, title, topic, diff;
    std::string desc, example, starter, support, helpers, solution, hint, prelude;
    std::vector<TestCase> tests;
    std::vector<ForbidRule> forbid;
};

struct ProblemSet {
    std::string id, title, subtitle, desc, prelude;
    int examNo = 0;
    Color color{ 80, 60, 40, 255 };
    std::vector<int> problems;   // indices into Content::problems
};

struct Content {
    std::vector<Topic> topics;
    std::vector<MCQ> mcqs;
    std::vector<Written> written;
    std::vector<Chapter> chapters;
    std::vector<Problem> problems;
    std::vector<ProblemSet> sets;
    std::vector<std::string> loadErrors;

    void Load();
    int TopicIndex(const std::string& id) const;
    const Topic* FindTopic(const std::string& id) const;
    const Chapter* ChapterFor(const std::string& topic) const;
    int ProblemIndex(const std::string& id) const;
    std::string TopicOfItem(const std::string& itemId) const;   // question / problem id -> topic id
    std::vector<int> McqsFor(const std::string& topic) const;
    std::vector<int> WrittenFor(const std::string& topic) const;
private:
    std::map<std::string, std::string> itemTopic;
    void LoadMcq(const std::string& path);
    void LoadWritten(const std::string& path);
    void LoadChapter(const std::string& path);
    void LoadProblems(const std::string& path);
};

Content& Data();
