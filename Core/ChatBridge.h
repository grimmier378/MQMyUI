#pragma once

#include <string>

class ChatBridge
{
public:
	void SetMyChatPresent(bool present) { m_myChatPresent = present; }
	bool IsMyChatPresent() const { return m_myChatPresent; }

	void DetectMyChat();

	void Print(const std::string& module, const std::string& text);

private:
	bool m_myChatPresent = false;
};
