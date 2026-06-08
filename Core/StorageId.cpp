#include "StorageId.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <vector>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

namespace myui
{
static unsigned long long Fnv1a(const std::string& s)
{
	unsigned long long hash = 1469598103934665603ULL;
	for (unsigned char c : s)
	{
		hash ^= c;
		hash *= 1099511628211ULL;
	}
	return hash;
}

std::string GetLanIPv4()
{
	const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
	ULONG size = 15000;
	std::vector<unsigned char> buffer(size);
	PIP_ADAPTER_ADDRESSES addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());

	ULONG ret = GetAdaptersAddresses(AF_INET, flags, nullptr, addrs, &size);
	if (ret == ERROR_BUFFER_OVERFLOW)
	{
		buffer.resize(size);
		addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
		ret = GetAdaptersAddresses(AF_INET, flags, nullptr, addrs, &size);
	}
	if (ret != NO_ERROR)
	{
		return "";
	}

	bool found = false;
	unsigned long best = 0;
	for (PIP_ADAPTER_ADDRESSES adapter = addrs; adapter != nullptr; adapter = adapter->Next)
	{
		if (adapter->OperStatus != IfOperStatusUp)
		{
			continue;
		}
		if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
		{
			continue;
		}
		for (PIP_ADAPTER_UNICAST_ADDRESS unicast = adapter->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next)
		{
			if (unicast->Address.lpSockaddr->sa_family != AF_INET)
			{
				continue;
			}
			sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(unicast->Address.lpSockaddr);
			unsigned long ip = ntohl(sin->sin_addr.s_addr);
			if ((ip >> 24) == 127)
			{
				continue;
			}
			if (!found || ip < best)
			{
				best = ip;
				found = true;
			}
		}
	}
	if (!found)
	{
		return "";
	}

	in_addr ia;
	ia.s_addr = htonl(best);
	char out[INET_ADDRSTRLEN] = {0};
	if (inet_ntop(AF_INET, &ia, out, sizeof(out)) == nullptr)
	{
		return "";
	}
	return std::string(out);
}

std::string GetHostNameStr()
{
	char buffer[256] = {0};
	DWORD size = sizeof(buffer);
	if (GetComputerNameA(buffer, &size))
	{
		return std::string(buffer, size);
	}
	return "";
}

std::string ComputeStorageId(const std::string& dbPath)
{
	std::error_code ec;
	std::filesystem::path absolute = std::filesystem::absolute(dbPath, ec);
	std::filesystem::path canonical = std::filesystem::weakly_canonical(absolute, ec);
	std::string path = ec ? dbPath : canonical.string();
	std::transform(path.begin(), path.end(), path.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	std::string key = GetLanIPv4() + "|" + path;
	unsigned long long hash = Fnv1a(key);

	static const char digits[] = "0123456789abcdef";
	char hex[17];
	for (int i = 0; i < 16; ++i)
	{
		hex[15 - i] = digits[(hash >> (i * 4)) & 0xF];
	}
	hex[16] = '\0';
	return std::string(hex);
}
} // namespace myui
