#pragma once
#include "url.h"
#include <memory>

class input_stream;
struct HttpRequest;
struct HttpOption;
struct FormList;
;
std::tuple<std::shared_ptr<HttpRequest>, std::shared_ptr<input_stream>>
openURL(const std::string &url, std::optional<Url> current,
        const HttpOption &option, FormList *request);
