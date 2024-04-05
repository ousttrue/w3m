#pragma once
#include <string>
#include <optional>

std::optional<int> openSocket(const std::string &hostname,
                              const std::string &remoteport_name,
                              unsigned short remoteport_num);
