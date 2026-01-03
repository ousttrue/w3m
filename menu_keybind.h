#pragma once
#include "defun.h"

enum MenuResult {
    MENU_NOTHING = -1,
    MENU_CANCEL = -2,
    MENU_CLOSE = -3,
};

typedef enum MenuResult (*MenuKeyFunc)(struct DefunContext ctx, char ch);

extern MenuKeyFunc MenuKeymap[128];
extern MenuKeyFunc MenuEscKeymap[128];
extern MenuKeyFunc MenuEscBKeymap[128];
extern MenuKeyFunc MenuEscDKeymap[128];

enum MenuResult mEsc(struct DefunContext ctx, char c);
enum MenuResult mEscB(struct DefunContext ctx, char c);
enum MenuResult mEscD(struct DefunContext ctx, char c);
enum MenuResult mNull(struct DefunContext ctx, char c);
enum MenuResult mSelect(struct DefunContext ctx, char c);
enum MenuResult mDown(struct DefunContext ctx, char c);
enum MenuResult mUp(struct DefunContext ctx, char c);
enum MenuResult mLast(struct DefunContext ctx, char c);
enum MenuResult mTop(struct DefunContext ctx, char c);
enum MenuResult mNext(struct DefunContext ctx, char c);
enum MenuResult mPrev(struct DefunContext ctx, char c);
enum MenuResult mFore(struct DefunContext ctx, char c);
enum MenuResult mBack(struct DefunContext ctx, char c);
enum MenuResult mLineU(struct DefunContext ctx, char c);
enum MenuResult mLineD(struct DefunContext ctx, char c);
enum MenuResult mOk(struct DefunContext ctx, char c);
enum MenuResult mCancel(struct DefunContext ctx, char c);
enum MenuResult mClose(struct DefunContext ctx, char c);
enum MenuResult mSusp(struct DefunContext ctx, char c);
enum MenuResult mMouse(struct DefunContext ctx, char c);
enum MenuResult mSgrMouse(struct DefunContext ctx, char c);
enum MenuResult mSrchF(struct DefunContext ctx, char c);
enum MenuResult mSrchB(struct DefunContext ctx, char c);
enum MenuResult mSrchN(struct DefunContext ctx, char c);
enum MenuResult mSrchP(struct DefunContext ctx, char c);
