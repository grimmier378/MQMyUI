#pragma once

#include <string>

class ChatBridge
{
public:
	void SetMyChatPresent(bool present);
	bool IsMyChatPresent() const;

	void DetectMyChat();

	void Print(const std::string& module, const std::string& text);
};

namespace myui
{
void ChatOut(const std::string& line);
void ChatOutf(const char* fmt, ...);
}
