#include "ChatBridge.h"

#include "../MyChatAPI.h"

#include <mq/Plugin.h>

#include <cstdarg>
#include <cstdio>

namespace
{
mqmychat::ChatAPI* s_chat = nullptr;

void RouteLine(const std::string& line)
{
	if (!s_chat || !s_chat->Send("MyUI", line))
	{
		WriteChatf("%s", line.c_str());
	}
}
}

void ChatBridge::SetMyChatPresent(bool present)
{
	if (present)
	{
		DetectMyChat();
	}
	else
	{
		s_chat = nullptr;
	}
}

bool ChatBridge::IsMyChatPresent() const
{
	return s_chat != nullptr;
}

void ChatBridge::DetectMyChat()
{
	s_chat = mqmychat::GetChatAPI();
}

void ChatBridge::Print(const std::string& module, const std::string& text)
{
	if (s_chat)
	{
		RouteLine(text);
	}
	else
	{
		WriteChatf("\at[%s]\ax %s", module.c_str(), text.c_str());
	}
}

namespace myui
{
void ChatOut(const std::string& line)
{
	RouteLine(line);
}

void ChatOutf(const char* fmt, ...)
{
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	RouteLine(buf);
}
}
