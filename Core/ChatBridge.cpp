#include "ChatBridge.h"

#include <mq/Plugin.h>

#include <algorithm>

void ChatBridge::DetectMyChat()
{
	m_myChatPresent = IsPluginLoaded("MQMyChat");
}

void ChatBridge::Print(const std::string& module, const std::string& text)
{
	if (m_myChatPresent)
	{
		std::string safe = text;
		std::replace(safe.begin(), safe.end(), ']', ')');
		EzCommand(fmt::format("/squelch /echo ${{MyChat.Send[MyUI,{}]}}", safe).c_str());
	}
	else
	{
		WriteChatf("\at[%s]\ax %s", module.c_str(), text.c_str());
	}
}
