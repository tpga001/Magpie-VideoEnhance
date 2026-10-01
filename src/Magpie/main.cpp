// Copyright (c) Xu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.


#include "pch.h"
#include "App.h"
#include "Win32Helper.h"
#include "TouchHelper.h"
#include "CommonSharedConstants.h"
#include "Logger.h"
#include <shellapi.h>

using namespace Magpie;
using namespace winrt::Magpie::implementation;

// 将当前目录设为程序所在目录
static void SetWorkingDir() noexcept {
	FAIL_FAST_IF_WIN32_BOOL_FALSE(SetCurrentDirectory(
		Win32Helper::GetExePath().parent_path().c_str()));
}

// Smooth Motion compatibility restarts launch the replacement before the old
// process has finished destroying D3D resources.  Wait here, before creating
// windows, mutexes, hooks or graphics devices, so the two instances never
// overlap.
static std::wstring NormalizeArgumentsAndWaitForParent() noexcept {
	static constexpr std::wstring_view WAIT_PREFIX = L"--wait-for-pid=";

	int argc = 0;
	wil::unique_hlocal_ptr<wchar_t*> argv(CommandLineToArgvW(GetCommandLineW(), &argc));
	if (!argv) {
		return {};
	}

	DWORD parentPid = 0;
	std::wstring normalized;
	for (int i = 1; i < argc; ++i) {
		const std::wstring_view argument = argv.get()[i];
		if (argument.starts_with(WAIT_PREFIX)) {
			const std::wstring pidText(argument.substr(WAIT_PREFIX.size()));
			wchar_t* end = nullptr;
			const unsigned long parsed = wcstoul(pidText.c_str(), &end, 10);
			if (end != pidText.c_str() && *end == L'\0' && parsed <= MAXDWORD) {
				parentPid = static_cast<DWORD>(parsed);
			}
		} else if (normalized.empty()) {
			normalized.assign(argument);
		}
	}

	if (parentPid && parentPid != GetCurrentProcessId()) {
		wil::unique_handle parent(OpenProcess(SYNCHRONIZE, FALSE, parentPid));
		if (parent) {
			WaitForSingleObject(parent.get(), INFINITE);
		}
	}

	return normalized;
}

static void InitializeLogger(const wchar_t* logFilePath) noexcept {
	// 最多两个日志文件，每个最多 500KB
	Logger::Get().Initialize(
		spdlog::level::info,
		logFilePath,
		CommonSharedConstants::LOG_MAX_SIZE,
		1
	);
}

int APIENTRY wWinMain(
	_In_ HINSTANCE /*hInstance*/,
	_In_opt_ HINSTANCE /*hPrevInstance*/,
	_In_ wchar_t* /*lpCmdLine*/,
	_In_ int /*nCmdShow*/
) {
#ifdef _DEBUG
	SetThreadDescription(GetCurrentThread(), L"Magpie-主线程");
#endif
	
	// 堆损坏时终止进程
	HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, nullptr, 0);

	SetWorkingDir();
	const std::wstring normalizedArguments = NormalizeArgumentsAndWaitForParent();
	const wchar_t* arguments = normalizedArguments.c_str();

	enum {
		Normal,
		RegisterTouchHelper,
		UnRegisterTouchHelper
	} mode = [&]() {
		if (arguments == L"-r"sv) {
			return RegisterTouchHelper;
		} else if (arguments == L"-ur"sv) {
			return UnRegisterTouchHelper;
		} else {
			return Normal;
		}
	}();

	InitializeLogger(mode == Normal ?
		CommonSharedConstants::LOG_PATH :
		CommonSharedConstants::REGISTER_TOUCH_HELPER_LOG_PATH);

	Logger::Get().Info(fmt::format("程序启动\n\t版本: {}\n\tOS 版本: {}\n\t管理员: {}",
#ifdef MP_VERSION_STRING
		STRINGIFY(MP_VERSION_STRING),
#elif defined(MP_COMMIT_ID)
		"dev (" STRINGIFY(MP_COMMIT_ID) ")",
#else
		"dev",
#endif
		Win32Helper::GetOSVersion().ToString<char>(),
		Win32Helper::IsProcessElevated() ? "是" : "否"
	));

	if (mode == RegisterTouchHelper) {
		// 使 TouchHelper 获得 UIAccess 权限
		return Magpie::TouchHelper::Register() ? 0 : 1;
	} else if (mode == UnRegisterTouchHelper) {
		return Magpie::TouchHelper::Unregister() ? 0 : 1;
	}

	// 程序结束时也不应调用 uninit_apartment
	// 见 https://kennykerr.ca/2018/03/24/cppwinrt-hosting-the-windows-runtime/
	winrt::init_apartment(winrt::apartment_type::single_threaded);

	auto& app = App::Get();
	if (!app.Initialize(arguments)) {
		return 0;
	}

	return app.Run();
}
