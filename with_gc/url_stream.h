#pragma once
#include "url.h"
#include <memory>

extern std::string index_file;
extern bool LocalhostOnly;

class input_stream;
struct HttpRequest;
struct HttpOption;
struct Form;
;
std::tuple<std::shared_ptr<HttpRequest>, std::shared_ptr<input_stream>>
openURL(const std::string &url, std::optional<Url> current,
        const HttpOption &option, const std::shared_ptr<Form> &form);
