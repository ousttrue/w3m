#pragma once

enum MenuResult {
    MENU_NOTHING = -1,
    MENU_CANCEL = -2,
    MENU_CLOSE = -3,
};

typedef enum MenuResult (*MenuKeyFunc)(char ch);

extern MenuKeyFunc MenuKeymap[128];
extern MenuKeyFunc MenuEscKeymap[128];
extern MenuKeyFunc MenuEscBKeymap[128];
extern MenuKeyFunc MenuEscDKeymap[128];

enum MenuResult mEsc(char c);
enum MenuResult mEscB(char c);
enum MenuResult mEscD(char c);
enum MenuResult mNull(char c);
enum MenuResult mSelect(char c);
enum MenuResult mDown(char c);
enum MenuResult mUp(char c);
enum MenuResult mLast(char c);
enum MenuResult mTop(char c);
enum MenuResult mNext(char c);
enum MenuResult mPrev(char c);
enum MenuResult mFore(char c);
enum MenuResult mBack(char c);
enum MenuResult mLineU(char c);
enum MenuResult mLineD(char c);
enum MenuResult mOk(char c);
enum MenuResult mCancel(char c);
enum MenuResult mClose(char c);
enum MenuResult mSusp(char c);
enum MenuResult mMouse(char c);
enum MenuResult mSgrMouse(char c);
enum MenuResult mSrchF(char c);
enum MenuResult mSrchB(char c);
enum MenuResult mSrchN(char c);
enum MenuResult mSrchP(char c);
