#pragma once
#include <source/includes.hpp>
#include <source/install/install.hpp>

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
	installer.CheckAndInstall();

	if (!launch.init())
		return 1;

	return 0;
}