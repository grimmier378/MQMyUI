#pragma once

#include <string>

namespace myui
{
std::string GetLanIPv4();
std::string GetHostNameStr();
std::string ComputeStorageId(const std::string& dbPath);
} // namespace myui
