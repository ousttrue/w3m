#pragma once
#include "http_request.h"
#include <optional>
#include <memory>

extern const char *DefaultType;
extern bool UseExternalDirBuffer;
#define CGI_EXTENSION ".cgi"
extern std::string DirBufferCommand;

struct Url;
struct FormList;
struct HttpOption;
struct HttpResponse;

std::shared_ptr<HttpResponse> loadGeneralFile(const std::string &path,
                                              std::optional<Url> current,
                                              const HttpOption &option,
                                              FormList *request = {});
