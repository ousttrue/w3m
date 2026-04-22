#pragma once

enum HistoryType {
    HistoryNone,
    HistoryLoad,
    HistorySave,
    HistoryURL,
    HistoryShell,
    HistoryText,
};

void initHist(void);
void unshiftHist(enum HistoryType hist, const char* ptr);
void pushHist(enum HistoryType hist, const char* ptr);
void pushUrlHist(const char* ptr);
bool hasHist(enum HistoryType hist, const char* ptr);
const char* lastHist(enum HistoryType hist);
const char* nextHist(enum HistoryType hist);
const char* prevHist(enum HistoryType hist);
int loadHistory(enum HistoryType hist);
void saveHistory(enum HistoryType hist);
const char* historyBuffer(enum HistoryType hist);
