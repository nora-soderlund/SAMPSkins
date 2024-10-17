#include "SAMP.h"
#include <Windows.h>
#include <sysinfoapi.h>
#include <memoryapi.h>
#include <libloaderapi.h>

#define SAMP_DLL		"samp.dll"
#define SAMP_CMP		"528D44240C508D7E09E8"

#define SAMP_INFO_OFFSET						0x2ACA24
#define SAMP_CHAT_INPUT_INFO_OFFSET					0x2ACA14
#define SAMP_FUNC_ADDCLIENTCMD						0x691B0

template<typename T>
T GetSAMPPtrInfo(uint32_t offset)
{
	if (g_dwSAMP_Addr == NULL)
		return NULL;
	return *(T*)(g_dwSAMP_Addr + offset);
}

struct stSAMP* stGetSampInfo(void)
{
	return GetSAMPPtrInfo<stSAMP*>(SAMP_INFO_OFFSET);
}

static signed char hex_to_dec(signed char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	if (ch >= 'a' && ch <= 'f')
		return ch - 'A' + 10;

	return -1;
}

uint8_t* hex_to_bin(const char* str)
{
	int		len = (int)strlen(str);
	uint8_t* buf, * sbuf;

	if (len == 0 || len % 2 != 0)
		return NULL;

	sbuf = buf = (uint8_t*)malloc(len / 2);

	while (*str)
	{
		signed char bh = hex_to_dec(*str++);
		signed char bl = hex_to_dec(*str++);

		if (bl == -1 || bh == -1)
		{
			free(sbuf);
			return NULL;
		}

		*buf++ = (uint8_t)(bl | (bh << 4));
	}

	return sbuf;
}

bool isBadPtr_handlerAny(void* pointer, ULONG size, DWORD dwFlags)
{
	DWORD						dwSize;
	MEMORY_BASIC_INFORMATION	meminfo;

	if (NULL == pointer)
		return true;

	memset(&meminfo, 0x00, sizeof(meminfo));
	dwSize = VirtualQuery(pointer, &meminfo, sizeof(meminfo));

	if (0 == dwSize)
		return true;

	if (MEM_COMMIT != meminfo.State)
		return true;

	if (0 == (meminfo.Protect & dwFlags))
		return true;

	if (size > meminfo.RegionSize)
		return true;

	if ((unsigned)((char*)pointer - (char*)meminfo.BaseAddress) > (unsigned)(meminfo.RegionSize - size))
		return true;

	return false;
}



bool isBadPtr_readAny(void* pointer, ULONG size)
{
	return isBadPtr_handlerAny(pointer, size, PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
		PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY);
}

bool isBadPtr_writeAny(void* pointer, ULONG size)
{
	return isBadPtr_handlerAny(pointer, size,
		PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY);
}

void* dll_baseptr_get(const char* dll_name)
{
	return GetModuleHandle(dll_name);
}

uint32_t getSampAddress()
{
	return (uint32_t)dll_baseptr_get(SAMP_DLL);
}

static int __page_size_get(void)
{
	static int	page_size = -1;
	SYSTEM_INFO si;

	if (page_size == -1)
	{
		GetSystemInfo(&si);
		page_size = (int)si.dwPageSize;
	}

	return page_size;
}
static int __page_write(void* _dest, const void* _src, uint32_t len)
{
	static int		page_size = __page_size_get();
	uint8_t* dest = (uint8_t*)_dest;
	const uint8_t* src = (const uint8_t*)_src;
	DWORD			prot_prev = 0;
	int				prot_changed = 0;
	int				ret = 1;

	while (len > 0)
	{
		ret = 1;
		int page_offset = (int)((UINT_PTR)dest % page_size);
		int page_remain = page_size - page_offset;
		int this_len = len;

		if (this_len > page_remain)
			this_len = page_remain;

		if (isBadPtr_writeAny(dest, this_len))
		{
			if (!VirtualProtect((void*)dest, this_len, PAGE_EXECUTE_READWRITE, &prot_prev))
				ret = 0;
			else
				prot_changed = 1;
		}

		if (ret)
			memcpy(dest, src, this_len);

		if (prot_changed)
		{
			DWORD	dummy;
			if (!VirtualProtect((void*)dest, this_len, prot_prev, &dummy))
				writeLog("__page_write() could not restore original permissions");
		}

		dest += this_len;
		src += this_len;
		len -= this_len;
	}

	return ret;
}

static int __page_read(void* _dest, const void* _src, uint32_t len)
{
	static int	page_size = __page_size_get();
	uint8_t* dest = (uint8_t*)_dest;
	uint8_t* src = (uint8_t*)_src;
	DWORD		prot_prev = 0;
	int			prot_changed = 0;
	int			ret = 1;

	while (len > 0)
	{
		ret = 1;
		int page_offset = (int)((UINT_PTR)src % page_size);
		int page_remain = page_size - page_offset;
		int this_len = len;

		if (this_len > page_remain)
			this_len = page_remain;

		if (isBadPtr_readAny(src, this_len))
		{
			if (!VirtualProtect((void*)src, this_len, PAGE_EXECUTE_READWRITE, &prot_prev))
				ret = 0;
			else
				prot_changed = 1;
		}

		if (ret)
			memcpy(dest, src, this_len);
		else
			memset(dest, 0, this_len);

		if (prot_changed)
		{
			DWORD	dummy;
			if (!VirtualProtect((void*)src, this_len, prot_prev, &dummy))
				writeLog("__page_read() could not restore original permissions");
		}

		dest += this_len;
		src += this_len;
		len -= this_len;
	}

	return ret;
}

int memcmp_safe(const void* _s1, const void* _s2, uint32_t len)
{
	const uint8_t* s1 = (const uint8_t*)_s1;
	const uint8_t* s2 = (const uint8_t*)_s2;
	uint8_t			buf[4096];

	for (;; )
	{
		if (len > 4096)
		{
			if (!memcpy_safe(buf, s1, 4096))
				return 0;
			if (memcmp(buf, s2, 4096))
				return 0;
			s1 += 4096;
			s2 += 4096;
			len -= 4096;
		}
		else
		{
			if (!memcpy_safe(buf, s1, len))
				return 0;
			if (memcmp(buf, s2, len))
				return 0;
			break;
		}
	}

	return 1;
}

int memcpy_safe(void* _dest, const void* _src, uint32_t len, int check, const void* checkdata)
{
	static int		page_size = __page_size_get();
	static int		recurse_ok = 1;
	uint8_t			buf[4096];
	uint8_t* dest = (uint8_t*)_dest;
	const uint8_t* src = (const uint8_t*)_src;
	int				ret = 1;

	if (check && checkdata)
	{
		if (!memcmp_safe(checkdata, _dest, len))
			return 0;
	}

	while (len > 0)
	{
		uint32_t	this_len = sizeof(buf);

		if (this_len > len)
			this_len = len;

		if (!__page_read(buf, src, this_len))
			ret = 0;

		if (!__page_write(dest, buf, this_len))
			ret = 0;

		len -= this_len;
		src += this_len;
		dest += this_len;
	}

	return ret;
}

void getSAMP()
{
	uint32_t samp_dll = getSampAddress();

	if (samp_dll != NULL)
	{
		g_dwSAMP_Addr = (uint32_t)samp_dll;

		if (g_dwSAMP_Addr != NULL)
		{
			if (memcmp_safe((uint8_t*)g_dwSAMP_Addr + 0xBABE, hex_to_bin(SAMP_CMP), 10))
			{
				writeLog("SAMP 0.3.DL was detected.");
			}
			else
			{
				writeLog("Unknown SA:MP version.");
			}
		}
	}
	else
	{
		writeLog("samp.dll not found.");
	}
}


struct stInputInfo* stGetInputInfo(void)
{
	return GetSAMPPtrInfo<stInputInfo*>(SAMP_CHAT_INPUT_INFO_OFFSET);
}

void addClientCommand(char* name, CMDPROC function)
{
	if (name == NULL || function == NULL) {
		writeLog("Command name or function is NULL.");

		return;
	}

	stInputInfo* g_Input = stGetInputInfo();

	if (g_Input == NULL) {
		writeLog("Input is NULL.");
		return;
	}

	if (g_Input->iCMDCount == (SAMP_MAX_CLIENTCMDS - 1))
	{
		writeLog("Error: couldn't initialize '" + std::string(name) + "'. Maximum command amount reached.");
		return;
	}

	if (strlen(name) > 30)
	{
		writeLog("Error: command name '" + std::string(name) + "' was too long.");
		return;
	}
	
	writeLog("Registering command " + std::string(name));

	((void(__thiscall*) (void* _this, char* command, CMDPROC function)) (g_dwSAMP_Addr + SAMP_FUNC_ADDCLIENTCMD)) (g_Input, name, function);
}

void addChatMessage(const char* string) {
	writeLog(string);

	((void(__thiscall*) (void* chatPtr, int messageType, const char* msg, char* prefix, DWORD, DWORD)) (g_dwSAMP_Addr + 0x67650)) (*(DWORD**)(g_dwSAMP_Addr + 0x2ACA10), 4, string, (char*)"", 0xFFFFFFF, 0xFFFFFFF);
};