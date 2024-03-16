#pragma once
#include "http_request.h"
#include <optional>
#include <memory>
#include <string_view>

extern const char *DefaultType;
extern bool UseExternalDirBuffer;

struct Url;
struct FormList;
struct HttpOption;
struct HttpResponse;

std::shared_ptr<HttpResponse> loadGeneralFile(const std::string &path,
                                              std::optional<Url> current,
                                              const HttpOption &option,
                                              FormList *request = {});
