#pragma once
#include "shared.h"

namespace VEH
{
	inline void* handler = nullptr;

	void AddHandler();
	void RemoveHandler();

	LONG CALLBACK ExceptionHandler(EXCEPTION_POINTERS* exception_info);
}