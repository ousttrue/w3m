#pragma once
#include "url.h"
#include "Str.h"

int find_auth_user_passwd(ParsedURL* pu, char* realm,
    pStr* uname, pStr* pwd, int is_proxy);
void add_auth_user_passwd(ParsedURL* pu, char* realm,
    pStr uname, pStr pwd, int is_proxy);
void invalidate_auth_user_passwd(ParsedURL* pu, char* realm,
    pStr uname, pStr pwd, int is_proxy);
