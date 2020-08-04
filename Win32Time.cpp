#include <stdint.h>
#include <windows.h>

#pragma comment(lib, "winmm.lib")

static struct InitPeriod
{
	InitPeriod()
	{
		timeBeginPeriod(1);
	}
} s_InitPeriod;

extern "C" uint64_t timeGetTime64();

uint64_t timeGetTime64()
{
	static thread_local uint64_t tick = 0;
	uint32_t diff = uint32_t(timeGetTime()) - uint32_t(tick);
	return tick += diff;
}
