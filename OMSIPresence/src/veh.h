#pragma once
#include "shared.h"

namespace veh
{
	inline void* handler = nullptr;

	void AddHandler();
	void RemoveHandler();

	LONG CALLBACK ExceptionHandler(EXCEPTION_POINTERS* exception_info);
}