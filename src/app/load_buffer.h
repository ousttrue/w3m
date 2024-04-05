#pragma once
#include <memory>
#include <string_view>

extern int squeezeBlankLine;

struct LineLayout;
struct HttpResponse;
void loadBuffer(const std::shared_ptr<LineLayout> &layout, int width,
                HttpResponse *res, std::string_view body);

std::shared_ptr<HttpResponse> getshell(const std::string &cmd);
