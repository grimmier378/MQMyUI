#pragma once

#include <string>

namespace myui
{
inline std::string TrimName(const char* name)
{
	if (!name)
	{
		return std::string();
	}

	std::string s(name);
	size_t pos = s.find_first_of("\r\n");
	if (pos != std::string::npos)
	{
		s.erase(pos);
	}
	return s;
}

inline int ColumnsForWidth(float avail, float cellSize)
{
	int cols = static_cast<int>(avail / (cellSize + 6.0f));
	return cols < 1 ? 1 : cols;
}
} // namespace myui
