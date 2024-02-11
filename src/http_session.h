#pragma once
#include "http_request.h"
#include <optional>
#include <memory>

extern const char *DefaultType;

struct Url;
struct FormList;
struct HttpOption;
struct HttpResponse;

std::shared_ptr<HttpResponse> loadGeneralFile(const char *path,
                                              std::optional<Url> current,
                                              const HttpOption &option,
                                              FormList *request = {});
