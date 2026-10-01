#include "pch.h"
#include "App.h"
#include "AppSettings.h"
#include "BrowserVideoGeometryPolicy.h"
#include "CommonSharedConstants.h"
#include "ConfigPersistence.h"
#include "EffectsService.h"
#include "ErrorService.h"
#include "Logger.h"
#include "ProfileService.h"
#include "Profile.h"
#include "ScalingMode.h"
#include "ScalingModesService.h"
#include "ScalingService.h"
#include "StrHelper.h"
#include "ShortcutService.h"
#include "ToastService.h"
#include "TouchHelper.h"
#include "Win32Helper.h"
#include "WindowHelper.h"
#include <rapidjson/document.h>
#include <dwmapi.h>
#include <cmath>
#include <cwctype>
#include <limits>

using namespace winrt::Magpie::implementation;
using namespace winrt;

using winrt::Magpie::ShortcutAction;

namespace Magpie {

namespace {

constexpr wchar_t BROWSER_VIDEO_MODE_NAME[] = L"浏览器视频 30fps → 当前屏幕原生60fps";
constexpr wchar_t BROWSER_VIDEO_REQUEST_FILE[] = L"browser-video-request.json";
constexpr auto BROWSER_VIDEO_FOREGROUND_GRACE = std::chrono::milliseconds(400);

struct BrowserVideoRequest {
	std::string action;
	std::string sessionId;
	HWND localWindow = NULL;
	DWORD localProcessId = 0;
	double left = 0;
	double top = 0;
	double width = 0;
	double height = 0;
	double innerWidth = 0;
	double innerHeight = 0;
	double devicePixelRatio = 1;
	double screenX = 0;
	double screenY = 0;
	double outerWidth = 0;
	double outerHeight = 0;
	std::wstring frameMapping;
	SIZE frameSize{};
	std::vector<std::array<double, 4>> uiRects;
};

static bool ReadNumber(const rapidjson::Value& object, const char* name, double& value) noexcept {
	const auto member = object.FindMember(name);
	if (member == object.MemberEnd() || !member->value.IsNumber()) return false;
	value = member->value.GetDouble();
	return std::isfinite(value);
}

static bool ParseBrowserVideoRequest(std::string_view json, BrowserVideoRequest& result) noexcept {
	rapidjson::Document document;
	document.Parse<rapidjson::kParseValidateEncodingFlag>(json.data(), json.size());
	if (document.HasParseError() || !document.IsObject()) return false;
	const auto action = document.FindMember("action");
	const auto session = document.FindMember("sessionId");
	if (action == document.MemberEnd() || !action->value.IsString() ||
		session == document.MemberEnd() || !session->value.IsString()) return false;
	result.action.assign(action->value.GetString(), action->value.GetStringLength());
	result.sessionId.assign(session->value.GetString(), session->value.GetStringLength());
	if (result.action == "disable") return true;
	if (result.action != "update") return false;
	const auto frame = document.FindMember("frameSource");
	if (frame != document.MemberEnd() && frame->value.IsObject()) {
		const auto name = frame->value.FindMember("mapName");
		double width = 0, height = 0;
		if (name == frame->value.MemberEnd() || !name->value.IsString() ||
			!ReadNumber(frame->value, "width", width) || !ReadNumber(frame->value, "height", height) ||
			width < 64 || height < 64 || width > 4096 || height > 4096 || width * height > 3840 * 2160 ||
			width != std::floor(width) || height != std::floor(height)) return false;
		std::string_view text(name->value.GetString(), name->value.GetStringLength());
		constexpr std::string_view prefix = "Local\\MagpieBrowserFrame_";
		if (!text.starts_with(prefix) || text.size() != prefix.size() + 32 ||
			!std::ranges::all_of(text.substr(prefix.size()), [](char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'); })) return false;
		result.frameMapping.assign(text.begin(), text.end());
		result.frameSize = { static_cast<LONG>(width), static_cast<LONG>(height) };
	}
	const auto rect = document.FindMember("rect");
	const auto local = document.FindMember("localPlayer");
	if (local != document.MemberEnd()) {
		if (!local->value.IsObject() || result.frameMapping.empty() ||
			!result.sessionId.starts_with("potplayer:")) return false;
		const auto hwnd = local->value.FindMember("hwnd");
		const auto pid = local->value.FindMember("pid");
		if (hwnd == local->value.MemberEnd() || !hwnd->value.IsUint64() ||
			pid == local->value.MemberEnd() || !pid->value.IsUint()) return false;
		result.localWindow = reinterpret_cast<HWND>(static_cast<uintptr_t>(hwnd->value.GetUint64()));
		result.localProcessId = pid->value.GetUint();
		return result.localWindow && result.localProcessId &&
			rect != document.MemberEnd() && rect->value.IsObject() &&
			ReadNumber(rect->value, "left", result.left) && ReadNumber(rect->value, "top", result.top) &&
			ReadNumber(rect->value, "width", result.width) && ReadNumber(rect->value, "height", result.height) &&
			std::abs(result.left) <= 100000 && std::abs(result.top) <= 100000 &&
			result.width >= 64 && result.height >= 64 && result.width <= 16384 && result.height <= 16384;
	}
	const auto ui = document.FindMember("uiRects");
	if (ui != document.MemberEnd() && ui->value.IsArray() && ui->value.Size() <= 96) {
		for (const auto& entry : ui->value.GetArray()) {
			if (!entry.IsArray() || entry.Size() != 4) return false;
			std::array<double, 4> item{};
			for (uint32_t i = 0; i < 4; ++i) {
				if (!entry[i].IsNumber()) return false;
				item[i] = entry[i].GetDouble();
				if (!std::isfinite(item[i]) || item[i] < 0 || item[i] > 1) return false;
			}
			if (item[2] > item[0] && item[3] > item[1]) result.uiRects.push_back(item);
		}
	}
	const auto viewport = document.FindMember("viewport");
	if (rect == document.MemberEnd() || !rect->value.IsObject() ||
		viewport == document.MemberEnd() || !viewport->value.IsObject()) return false;
	return ReadNumber(rect->value, "left", result.left) &&
		ReadNumber(rect->value, "top", result.top) &&
		ReadNumber(rect->value, "width", result.width) &&
		ReadNumber(rect->value, "height", result.height) &&
		ReadNumber(viewport->value, "innerWidth", result.innerWidth) &&
		ReadNumber(viewport->value, "innerHeight", result.innerHeight) &&
		ReadNumber(viewport->value, "devicePixelRatio", result.devicePixelRatio) &&
		ReadNumber(viewport->value, "screenX", result.screenX) &&
		ReadNumber(viewport->value, "screenY", result.screenY) &&
		ReadNumber(viewport->value, "outerWidth", result.outerWidth) &&
		ReadNumber(viewport->value, "outerHeight", result.outerHeight) &&
		result.width >= 64 && result.height >= 64 &&
		result.innerWidth >= 64 && result.innerHeight >= 64 &&
		result.devicePixelRatio >= 0.5 && result.devicePixelRatio <= 8;
}

static bool IsSupportedChromiumWindow(HWND window) noexcept {
	if (!window || !IsWindow(window) || !IsWindowVisible(window)) return false;
	const std::wstring className = Win32Helper::GetWindowClassName(window);
	if (!className.starts_with(L"Chrome_WidgetWin_")) return false;
	std::wstring fileName = std::filesystem::path(Win32Helper::GetWindowPath(window)).filename().native();
	std::ranges::transform(fileName, fileName.begin(), [](wchar_t value) {
		return static_cast<wchar_t>(std::towlower(value));
	});
	return fileName == L"msedge.exe" || fileName == L"chrome.exe" ||
		fileName == L"brave.exe" || fileName == L"vivaldi.exe" || fileName == L"opera.exe";
}

struct BrowserWindowSearch {
	const BrowserVideoRequest* request;
	HWND foreground;
	HWND best = NULL;
	double bestScore = std::numeric_limits<double>::max();
};

static BOOL CALLBACK FindBrowserWindowCallback(HWND window, LPARAM parameter) noexcept {
	auto& search = *reinterpret_cast<BrowserWindowSearch*>(parameter);
	if (!IsSupportedChromiumWindow(window) || GetWindow(window, GW_OWNER)) return TRUE;
	RECT client{};
	RECT frame{};
	if (!Win32Helper::GetClientScreenRect(window, client) || FAILED(DwmGetWindowAttribute(
		window, DWMWA_EXTENDED_FRAME_BOUNDS, &frame, sizeof(frame)))) return TRUE;
	const double scale = GetDpiForWindow(window) / 96.0;
	const double expectedInnerWidth = search.request->innerWidth * scale;
	const double expectedInnerHeight = search.request->innerHeight * scale;
	const double clientWidth = client.right - client.left;
	const double clientHeight = client.bottom - client.top;
	if (clientWidth + 8 < expectedInnerWidth || clientHeight + 8 < expectedInnerHeight) return TRUE;

	const double frameWidth = frame.right - frame.left;
	const double frameHeight = frame.bottom - frame.top;
	const double sizeScore = std::abs(frameWidth - search.request->outerWidth * scale) +
		std::abs(frameHeight - search.request->outerHeight * scale);
	const double scaledPositionScore = std::abs(frame.left - search.request->screenX * scale) +
		std::abs(frame.top - search.request->screenY * scale);
	const double rawPositionScore = std::abs(frame.left - search.request->screenX) +
		std::abs(frame.top - search.request->screenY);
	const double chromeHeight = clientHeight - expectedInnerHeight;
	const double geometryPenalty = chromeHeight < -4 || chromeHeight > 500 * scale ? 100000 : 0;
	const double foregroundBonus = window == search.foreground ? -200 : 0;
	const double score = sizeScore * 4 + (std::min)(scaledPositionScore, rawPositionScore) +
		geometryPenalty + foregroundBonus;
	if (score < search.bestScore) {
		search.bestScore = score;
		search.best = window;
	}
	return TRUE;
}

static HWND FindBrowserWindow(const BrowserVideoRequest& request) noexcept {
	HWND foreground = GetAncestor(GetForegroundWindow(), GA_ROOT);
	BrowserWindowSearch search{ .request = &request, .foreground = foreground };
	EnumWindows(FindBrowserWindowCallback, reinterpret_cast<LPARAM>(&search));
	return search.best;
}

static bool IsSupportedLocalPlayer(const BrowserVideoRequest& request) noexcept {
	const HWND window = request.localWindow;
	if (!window || !IsWindowVisible(window) || GetAncestor(window, GA_ROOT) != window ||
		Win32Helper::GetWindowClassName(window) != L"PotPlayer64") return false;
	DWORD pid = 0;
	GetWindowThreadProcessId(window, &pid);
	std::wstring name = std::filesystem::path(Win32Helper::GetWindowPath(window)).filename().native();
	std::ranges::transform(name, name.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
	return pid == request.localProcessId && name == L"potplayermini64.exe";
}

static bool CalculateBrowserVideoCropping(HWND window, const BrowserVideoRequest& request,
	Cropping& cropping, HMONITOR& monitor, SIZE& videoSize) noexcept {
	RECT client{};
	RECT frame{};
	if (!Win32Helper::GetClientScreenRect(window, client) || FAILED(DwmGetWindowAttribute(
		window, DWMWA_EXTENDED_FRAME_BOUNDS, &frame, sizeof(frame)))) return false;
	const UINT dpi = GetDpiForWindow(window);
	const double scale = dpi / 96.0;
	const LONG clientWidth = client.right - client.left;
	const LONG clientHeight = client.bottom - client.top;
	const LONG viewportWidth = std::lround(request.innerWidth * scale);
	const LONG viewportHeight = std::lround(request.innerHeight * scale);
	if (!request.localWindow && (viewportWidth < 64 || viewportHeight < 64 ||
		viewportWidth > clientWidth + 8 || viewportHeight > clientHeight + 8)) return false;

	// Chromium exposes the page viewport size but not its native screen origin.
	// The viewport reaches the bottom of the client area. Any horizontal chrome
	// (for example vertical tabs) is placed to the left by default.
	const LONG viewportLeft = client.right - (std::min)(viewportWidth, clientWidth);
	const LONG viewportTop = client.bottom - (std::min)(viewportHeight, clientHeight);
	RECT video{
		viewportLeft + std::lround(request.left * scale),
		viewportTop + std::lround(request.top * scale),
		viewportLeft + std::lround((request.left + request.width) * scale),
		viewportTop + std::lround((request.top + request.height) * scale)
	};
	if (request.localWindow) video = {
		std::lround(request.left), std::lround(request.top),
		std::lround(request.left + request.width), std::lround(request.top + request.height)
	};

	WINDOWPLACEMENT placement{ .length = sizeof(placement) };
	if (!GetWindowPlacement(window, &placement)) return false;
	LONG border = placement.showCmd == SW_SHOWMAXIMIZED ? 0 :
		static_cast<LONG>(Win32Helper::GetNativeWindowBorderThickness(dpi));
	RECT source{
		frame.left + border,
		(std::max)(frame.top + border, client.top),
		frame.right - border,
		frame.bottom - border
	};
	if (request.localWindow) {
		BOOL nativeFrame = TRUE;
		if (FAILED(DwmGetWindowAttribute(window, DWMWA_NCRENDERING_ENABLED, &nativeFrame, sizeof(nativeFrame))) || !nativeFrame) {
			// PotPlayer draws its own frame. Match SrcTracker's uncropped origin.
			source = client;
			if (!(GetWindowLongPtrW(window, GWL_STYLE) & (WS_CAPTION | WS_THICKFRAME)) &&
				!GetWindowRect(window, &source)) return false;
		}
	}
	if (placement.showCmd == SW_SHOWMAXIMIZED) {
		MONITORINFO info{ .cbSize = sizeof(info) };
		if (GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &info)) {
			IntersectRect(&source, &source, &info.rcMonitor);
		}
	}
	RECT clipped{};
	if (!IntersectRect(&clipped, &video, &source) ||
		clipped.right - clipped.left < 64 || clipped.bottom - clipped.top < 64) return false;

	cropping.Left = static_cast<float>(clipped.left - source.left);
	cropping.Top = static_cast<float>(clipped.top - source.top);
	cropping.Right = static_cast<float>(source.right - clipped.right);
	cropping.Bottom = static_cast<float>(source.bottom - clipped.bottom);
	monitor = MonitorFromRect(&clipped, MONITOR_DEFAULTTONEAREST);
	videoSize = Win32Helper::GetSizeOfRect(clipped);
	return true;
}

static SIZE CalculateBrowserVideoProcessingSize(SIZE videoSize, HMONITOR monitor) noexcept {
	MONITORINFO info{ .cbSize = sizeof(info) };
	if (!monitor || !GetMonitorInfoW(monitor, &info) || videoSize.cx <= 0 || videoSize.cy <= 0) {
		return videoSize;
	}

	const SIZE monitorSize = Win32Helper::GetSizeOfRect(info.rcMonitor);
	const double fitScale = (std::min)(
		static_cast<double>(monitorSize.cx) / videoSize.cx,
		static_cast<double>(monitorSize.cy) / videoSize.cy);
	const double scale = (std::max)(1.0, fitScale);
	LONG width = static_cast<LONG>(std::lround(videoSize.cx * scale));
	LONG height = static_cast<LONG>(std::lround(videoSize.cy * scale));
	// Native video backends and frame generation behave more predictably with
	// even dimensions. Rounding down by one pixel keeps the fitted aspect and
	// never exceeds the selected display.
	width = (std::max)(64L, width & ~1L);
	height = (std::max)(64L, height & ~1L);
	return { width, height };
}

}

static ToolbarShortcutLabels GetToolbarShortcutLabels() {
	const auto& settings = AppSettings::Get();
	auto label = [&](ShortcutAction action) {
		return StrHelper::UTF16ToUTF8(settings.GetShortcut(action).ToString());
	};
	return {
		.profiler = label(ShortcutAction::Profiler),
		.parameters = label(ShortcutAction::EffectParameters),
		.screenshot = label(ShortcutAction::Screenshot),
		.pin = label(ShortcutAction::ToolbarPin),
		.comparison = label(ShortcutAction::Comparison),
		.fullscreen = label(ShortcutAction::Scale),
		.windowed = label(ShortcutAction::WindowedModeScale)
	};
}

ScalingService& ScalingService::Get() noexcept {
	static ScalingService instance;
	return instance;
}

ScalingService::~ScalingService() {}

void ScalingService::Initialize() {
	_scalingRuntime.emplace();
	_scalingRuntime->StateChanged(
		std::bind_front(&ScalingService::_ScalingRuntime_StateChanged, this));

	const DispatcherQueue& dispatcher = App::Get().Dispatcher();

	_countDownTimer = dispatcher.CreateTimer();
	_countDownTimer.Interval(25ms);
	_countDownTimer.Tick({ this, &ScalingService::_CountDownTimer_Tick });

	_checkForegroundTimer = dispatcher.CreateTimer();
	_checkForegroundTimer.Interval(50ms);
	_checkForegroundTimer.Tick({ this, &ScalingService::_CheckForegroundTimer_Tick });
	_checkForegroundTimer.Start();

	_shortcutActivatedRevoker = ShortcutService::Get().ShortcutActivated(
		auto_revoke, std::bind_front(&ScalingService::_ShortcutService_ShortcutPressed, this));
	_toolbarShortcutsChangedRevoker = AppSettings::Get().ShortcutChanged(auto_revoke, [this](ShortcutAction) {
		if (_scalingRuntime) _scalingRuntime->UpdateToolbarShortcutLabels(GetToolbarShortcutLabels());
	});
	_frameSyncChangedRevoker = AppSettings::Get().FrontEdgeSyncChanged(auto_revoke, [this] {
		const auto& settings = AppSettings::Get();
		if (_scalingRuntime) _scalingRuntime->UpdateFrameSyncSettings(
			{ settings.IsFrontEdgeSyncEnabled(), settings.FrontEdgeSyncFrameRate(), settings.GetFrameSyncMode() });
	});

	// 立即检查前台窗口
	_CheckForegroundTimer_Tick(nullptr, nullptr);
}

void ScalingService::Uninitialize() {
	if (!_scalingRuntime) {
		return;
	}

	_checkForegroundTimer.Stop();
	_countDownTimer.Stop();
	_frameSyncChangedRevoker.Revoke();
	_toolbarShortcutsChangedRevoker.Revoke();
	_scalingRuntime.reset();
	// The runtime destructor drains UI requests before this final flush.
	_FlushEffectParametersSaves(true);
	// Also wait for any snapshot already being written in the background. The
	// newest synchronous revision includes requests drained during shutdown.
	AppSettings::Get().Save();

	_shortcutActivatedRevoker.Revoke();
}

void ScalingService::StartTimer(bool windowedMode) {
	_curCountdownSeconds = AppSettings::Get().CountdownSeconds();
	_isCurCountdownWindowedMode = windowedMode;
	_timerStartTimePoint = std::chrono::steady_clock::now();
	// 如果计时器已经启动会被重置，这正是我们想要的
	_countDownTimer.Start();
	IsTimerOnChanged.Invoke(true, windowedMode);
}

void ScalingService::StopTimer() {
	if (_curCountdownSeconds == 0) {
		return;
	}

	_curCountdownSeconds = 0;
	_countDownTimer.Stop();
	IsTimerOnChanged.Invoke(false, _isCurCountdownWindowedMode);
}

double ScalingService::SecondsLeft() const noexcept {
	using namespace std::chrono;

	if (!IsTimerOn()) {
		return std::numeric_limits<double>::max();
	}

	// DispatcherTimer 误差很大，因此我们自己计算剩余时间
	auto now = steady_clock::now();
	int msLeft = (int)duration_cast<milliseconds>(_timerStartTimePoint + seconds(_curCountdownSeconds) - now).count();
	return msLeft / 1000.0;
}

bool ScalingService::IsScaling() const noexcept {
	// 等待状态视为未缩放
	return _scalingRuntime->State() == ScalingState::Scaling;
}

void ScalingService::CheckForeground() {
	_hwndChecked = NULL;
	_CheckForegroundTimer_Tick(nullptr, nullptr);
}

void ScalingService::HandleBrowserVideoRequest() noexcept {
	const std::filesystem::path requestPath =
		AppSettings::Get().ConfigDir() / BROWSER_VIDEO_REQUEST_FILE;
	std::string requestText;
	uint32_t error = 0;
	if (!ConfigPersistence::ReadFileBytes(requestPath, requestText, &error)) {
		if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return;
		Logger::Get().Error(fmt::format("Browser video request read failed: {}", error));
		return;
	}
	std::error_code removeError;
	std::filesystem::remove(requestPath, removeError);
	BrowserVideoRequest request;
	if (!ParseBrowserVideoRequest(ConfigPersistence::JsonText(requestText), request)) {
		Logger::Get().Error("Browser video request is invalid");
		return;
	}
	if (request.action == "disable") {
		if (_browserVideoAutoActive &&
			(request.sessionId == "global" || request.sessionId == _browserVideoSession)) {
			_browserVideoAutoActive = false;
			_browserVideoSession.clear();
			_browserVideoWindow = NULL;
			_browserVideoMonitor = NULL;
			_browserVideoProcessingSize = {};
			_browserVideoForegroundLostAt = {};
			_RetireBrowserFrameLease();
			_scalingRuntime->Stop();
			Logger::Get().Info("Browser video auto mode stopped");
		}
		return;
	}

	HWND window = request.localWindow ? (IsSupportedLocalPlayer(request) ? request.localWindow : NULL) :
		(request.sessionId == _browserVideoSession && IsSupportedChromiumWindow(_browserVideoWindow) ?
			_browserVideoWindow : FindBrowserWindow(request));
	if (!window) {
		Logger::Get().Error("Browser video auto mode could not match the reporting browser window");
		return;
	}
	// Native foreground authority is checked again after extension/native-host
	// queues: a report captured before Alt+Tab must never start an overlay later.
	if (GetAncestor(GetForegroundWindow(), GA_ROOT) != window) {
		if (_browserVideoAutoActive && window == _browserVideoWindow &&
			_browserVideoForegroundLostAt == std::chrono::steady_clock::time_point{}) {
			_browserVideoForegroundLostAt = std::chrono::steady_clock::now();
		}
		return;
	}
	// While the user is dragging a window border, show the original browser.
	// Rebuild from a fresh DOM report after sizing ends, never from old margins.
	GUITHREADINFO guiInfo{ .cbSize = sizeof(guiInfo) };
	if (IsIconic(window) || (GetGUIThreadInfo(GetWindowThreadProcessId(window, nullptr), &guiInfo) &&
		(guiInfo.flags & GUI_INMOVESIZE))) {
		if (_browserVideoAutoActive) _scalingRuntime->Stop();
		return;
	}
	Cropping cropping{};
	HMONITOR monitor = NULL;
	SIZE videoSize{};
	if (!CalculateBrowserVideoCropping(window, request, cropping, monitor, videoSize)) {
		Logger::Get().Error("Browser video auto mode received an invalid or off-window video rectangle");
		return;
	}
	std::vector<RECT> uiRects;
	if (!request.frameMapping.empty()) for (const auto& item : request.uiRects) uiRects.push_back({
		static_cast<LONG>(std::floor(item[0] * videoSize.cx)), static_cast<LONG>(std::floor(item[1] * videoSize.cy)),
		static_cast<LONG>(std::ceil(item[2] * videoSize.cx)), static_cast<LONG>(std::ceil(item[3] * videoSize.cy)) });
	const SIZE frameSize = request.frameMapping.empty() ? videoSize : request.frameSize;
	const SIZE processingSize = CalculateBrowserVideoProcessingSize(frameSize, monitor);
	const BrowserVideoGeometrySample currentGeometry{
		.overlaySize = _browserVideoSize,
		.frameSize = _browserFrameMapping.empty() ? _browserVideoSize : _browserFrameSize,
		.processingSize = _browserVideoProcessingSize,
		.cropping = { _browserVideoCropping.Left, _browserVideoCropping.Top,
			_browserVideoCropping.Right, _browserVideoCropping.Bottom }
	};
	const BrowserVideoGeometrySample candidateGeometry{
		.overlaySize = videoSize,
		.frameSize = frameSize,
		.processingSize = processingSize,
		.cropping = { cropping.Left, cropping.Top, cropping.Right, cropping.Bottom }
	};
	if (_browserVideoAutoActive && request.sessionId == _browserVideoSession &&
		window == _browserVideoWindow && monitor == _browserVideoMonitor &&
		request.frameMapping == _browserFrameMapping &&
		CanReuseBrowserVideoGeometry(currentGeometry, candidateGeometry) &&
		_scalingRuntime->State() != ScalingState::Idle) {
		_scalingRuntime->UpdateBrowserUiRects(std::move(uiRects));
		return;
	}

	const auto& modes = AppSettings::Get().ScalingModes();
	const auto mode = std::ranges::find(modes, std::wstring_view(BROWSER_VIDEO_MODE_NAME), &ScalingMode::name);
	if (mode == modes.end()) {
		Logger::Get().Error("Browser video auto mode is missing its RTX VSR + 60 fps effect group");
		return;
	}
	BrowserFrameLease frameLease;
	if (!request.frameMapping.empty()) {
		if (request.frameMapping == _browserFrameMapping &&
			_browserFrameLease.mapping && _browserFrameLease.mutex) {
			frameLease = std::move(_browserFrameLease);
		} else {
			frameLease.mapping.reset(OpenFileMappingW(FILE_MAP_READ, FALSE, request.frameMapping.c_str()));
			if (!frameLease.mapping) {
				Logger::Get().Win32Error("Acquire browser decoded frame mapping lease failed");
				return;
			}
			frameLease.mutex.reset(OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE,
				(request.frameMapping + L"_Mutex").c_str()));
			if (!frameLease.mutex) {
				Logger::Get().Win32Error("Acquire browser decoded frame mutex lease failed");
				return;
			}
		}
	}
	Profile profile = *ProfileService::Get().GetProfileForWindow(window, false);
	profile.scalingMode = static_cast<int>(mode - modes.begin());
	profile.autoScale = AutoScale::Disabled;
	profile.isCroppingEnabled = true;
	profile.cropping = cropping;
	profile.isFrameRateLimiterEnabled = true;
	profile.maxFrameRate = 30.0f;
	profile.multiMonitorUsage = MultiMonitorUsage::Closest;
	profile.IsCaptureTitleBar(false);
	profile.IsAdjustCursorSpeed(false);
	profile.Is3DGameMode(false);

	_browserVideoSession = request.sessionId;
	_browserVideoWindow = window;
	_browserVideoCropping = cropping;
	_browserVideoSize = videoSize;
	_browserFrameMapping = request.frameMapping;
	_browserFrameSize = request.frameSize;
	_browserVideoProcessingSize = processingSize;
	_browserUiRects = std::move(uiRects);
	if (_browserFrameLease.mapping || _browserFrameLease.mutex) {
		_retiredBrowserFrameLeases.push_back(std::move(_browserFrameLease));
	}
	_browserFrameLease = std::move(frameLease);
	_browserVideoMonitor = monitor;
	_browserVideoAutoActive = true;
	_browserVideoForegroundLostAt = {};
	_isAutoScaleSuspended = false;
	_StartScale(window, profile, false, true, processingSize);
	Logger::Get().Info(fmt::format(
		"{} video auto mode: hwnd={} crop=({:.0f},{:.0f},{:.0f},{:.0f}) "
		"overlay={}x{} processing={}x{}",
		request.localWindow ? "Local player" : "Browser",
		reinterpret_cast<uintptr_t>(window), cropping.Left, cropping.Top, cropping.Right, cropping.Bottom,
		videoSize.cx, videoSize.cy, processingSize.cx, processingSize.cy));
}

void ScalingService::OnTaskSwitch() {
	if (!AppSettings::Get().IsStopEffectsOnTaskSwitchEnabled()) return;
	if (!_scalingRuntime || !_scalingRuntime->StopForTaskSwitch()) return;
	_isAutoScaleSuspended = true;
	StopTimer();
	Logger::Get().Info("Task switch: stopping fullscreen effects; waiting for a manual start");
}

void ScalingService::_ShortcutService_ShortcutPressed(ShortcutAction action) {
	switch (action) {
	case ShortcutAction::Scale:
	case ShortcutAction::WindowedModeScale:
	{
		const bool isWindowdMode = action == ShortcutAction::WindowedModeScale;

		const ScalingState state = _scalingRuntime->State();
		if (state == ScalingState::Scaling) {
			_scalingRuntime->ToggleScaling(isWindowdMode);
		} else if (state == ScalingState::Starting) {
			// A repeated hotkey during initialization means cancel this run. The
			// runtime generation prevents the queued stop from affecting a later run.
			_scalingRuntime->Stop();
		} else if (state == ScalingState::Stopping) {
			return;
		} else {
			_ScaleForegroundWindow(isWindowdMode);
		}

		break;
	}
	case ShortcutAction::Profiler:
		_scalingRuntime->InvokeOverlayAction(OverlayAction::Profiler);
		break;
	case ShortcutAction::EffectParameters:
		_scalingRuntime->InvokeOverlayAction(OverlayAction::EffectParameters);
		break;
	case ShortcutAction::Screenshot:
		_scalingRuntime->InvokeOverlayAction(OverlayAction::Screenshot);
		break;
	case ShortcutAction::ToolbarPin:
		_scalingRuntime->InvokeOverlayAction(OverlayAction::ToolbarPin);
		break;
	case ShortcutAction::Comparison:
		_scalingRuntime->InvokeOverlayAction(OverlayAction::Comparison);
		break;
	case ShortcutAction::Toolbar:
	{
		_scalingRuntime->SwitchToolbarState();
		break;
	}
	default:
		break;
	}
}

void ScalingService::_CountDownTimer_Tick(winrt::DispatcherQueueTimer const&, winrt::IInspectable const&) {
	const double timeLeft = SecondsLeft();

	// 剩余时间在 10 ms 以内计时结束
	if (timeLeft < 0.01) {
		StopTimer();
		_ScaleForegroundWindow(_isCurCountdownWindowedMode);
		return;
	}

	TimerTick.Invoke(timeLeft);
}

static IssueContext MakeIssueContext(HWND window, const Profile& profile) {
	IssueContext result;
	result.hasProfile = true;
	result.profileName = profile.name;
	result.profilePath = profile.pathRule;
	result.profileClass = profile.classNameRule;
	result.captureMethod = profile.captureMethod;
	const auto& modes = AppSettings::Get().ScalingModes();
	if (profile.scalingMode >= 0 && static_cast<size_t>(profile.scalingMode) < modes.size())
		result.scalingModeName = modes[profile.scalingMode].name;
	wchar_t title[512]{};
	GetWindowTextW(window, title, static_cast<int>(std::size(title)));
	result.windowTitle = title;
	return result;
}

static bool IsPopupWindow(HWND hwndPopup, HWND hwndOwner) noexcept {
	// 检查所有者关系
	{
		HWND hwndCur = hwndPopup;
		while (bool(hwndCur = GetWindowOwner(hwndCur))) {
			if (hwndCur == hwndOwner) {
				return true;
			}
		}
	}

	// 有些游戏不用所有者关系来实现弹窗，而是将主窗口禁用，做到和模态弹窗差不多的效果。
	// 这不可能准确检测，只能尽可能增加限制以减少误判，我们检查三个条件：
	//
	// 1. 主窗口处于禁用状态
	// 2. 两个窗口位于同一个进程
	// 3. 主窗口没有传统意义的弹窗

	if (IsWindowEnabled(hwndOwner)) {
		return false;
	}

	DWORD pid1 = 0;
	DWORD pid2 = 0;
	GetWindowThreadProcessId(hwndPopup, &pid1);
	GetWindowThreadProcessId(hwndOwner, &pid2);
	if (pid1 != pid2) {
		return false;
	}

	return !GetWindow(hwndOwner, GW_ENABLEDPOPUP);
}

static bool IsReadyForScaling(HWND hwndFore) noexcept {
	// GH#1148
	// 有些游戏刚启动时将窗口创建在屏幕外，初始化完成后再移到屏幕内
	if (!MonitorFromWindow(hwndFore, MONITOR_DEFAULTTONULL)) {
		return false;
	}

	// GH#1200
	// 有些游戏加载时不响应消息，应等待加载完成
	return !Win32Helper::IsWindowHung(hwndFore);
}

void ScalingService::_CheckForegroundTimer_Tick(winrt::DispatcherQueueTimer const&, winrt::IInspectable const&) {
	const HWND hwndFore = GetForegroundWindow();
	if (_browserVideoAutoActive) {
		if (GetAncestor(hwndFore, GA_ROOT) == _browserVideoWindow) {
			_browserVideoForegroundLostAt = {};
		} else {
			const auto now = std::chrono::steady_clock::now();
			if (_browserVideoForegroundLostAt == std::chrono::steady_clock::time_point{}) {
				_browserVideoForegroundLostAt = now;
			} else if (now - _browserVideoForegroundLostAt >= BROWSER_VIDEO_FOREGROUND_GRACE) {
				_browserVideoAutoActive = false;
				_browserVideoForegroundLostAt = {};
				_RetireBrowserFrameLease();
				_scalingRuntime->Stop();
				Logger::Get().Info("Browser video overlay suspended: source is not foreground");
			}
		}
	} else {
		_browserVideoForegroundLostAt = {};
	}
	if (_isAutoScaleSuspended) return;
	if (!hwndFore || hwndFore == _hwndChecked) {
		return;
	}

	if (hwndFore != _hwndCurSrc) {
		// 检查自动缩放
		const Profile* profile = ProfileService::Get().GetProfileForWindow(hwndFore, true);
		// 正在缩放窗口时禁止自动缩放它的弹窗
		if (profile && !(_hwndCurSrc && IsPopupWindow(hwndFore, _hwndCurSrc))) {
			// 如果窗口处于某种中间状态则跳过此次检查
			if (!IsReadyForScaling(hwndFore)) {
				return;
			}

			// 自动缩放可以终止当前缩放
			_StartScale(hwndFore, *profile, profile->autoScale == AutoScale::Windowed, true);
		}
	}

	// 避免重复检查
	_hwndChecked = hwndFore;
}

void ScalingService::_RetireBrowserFrameLease() {
	if (_browserFrameLease.mapping || _browserFrameLease.mutex) {
		_retiredBrowserFrameLeases.push_back(std::move(_browserFrameLease));
	}
}

void ScalingService::_ScalingRuntime_StateChanged(ScalingState value) {
	App::Get().Dispatcher().TryEnqueue([this, value]() {
		bool shouldRestartForSmoothMotion = false;

		if (value == ScalingState::Scaling) {
			StopTimer();
		} else if (value == ScalingState::Idle) {
			_FlushEffectParametersSaves();
			// A stop can overtake the asynchronously dispatched renderer start.
			// Keep replaced mappings alive until that run has fully reached Idle.
			_retiredBrowserFrameLeases.clear();
			shouldRestartForSmoothMotion = !_isCurBrowserVideoScale && !_isAutoScaleSuspended && _hwndCurSrc &&
				AppSettings::Get().IsSmoothMotionCompatibilityMode();

			// 缩放结束后源窗口位于前台则不要检查自动缩放，用户可能刚通过快捷键或
			// 工具栏终止缩放。_CheckForegroundTimer_Tick 也实现了类似功能，但它
			// 的触发频率较低，容易错过时机。
			if (GetForegroundWindow() == _hwndCurSrc) {
				_hwndChecked = _hwndCurSrc;
			}

			// 缩放结束后清空 _hwndCurSrc，等待状态下则保留
			_hwndCurSrc = NULL;
			_isCurBrowserVideoScale = false;
		}

		IsScalingChanged.Invoke(value == ScalingState::Scaling);

		if (shouldRestartForSmoothMotion) {
			Logger::Get().Info("Smooth Motion 兼容模式：缩放结束后重启 Magpie");
			AppSettings::Get().Save();
			App::Get().RestartForSmoothMotion();
		}
	});
}

void ScalingService::_ScaleForegroundWindow(bool windowedMode) {
	const HWND hWnd = GetForegroundWindow();
	if (!hWnd) {
		return;
	}

	const Profile& profile = *ProfileService::Get().GetProfileForWindow(hWnd, false);
	_StartScale(hWnd, profile, windowedMode, false);
}

void ScalingService::_StartScale(HWND hWnd, const Profile& profile, bool windowedMode, bool force,
	SIZE regionOverlayInternalSize) {
	assert(hWnd);

	const ScalingError error = _StartScaleImpl(
		hWnd, profile, windowedMode, force, regionOverlayInternalSize);
	if (error != ScalingError::NoError) {
		std::string context;
		const auto& modes = AppSettings::Get().ScalingModes();
		if (profile.scalingMode >= 0 && static_cast<size_t>(profile.scalingMode) < modes.size()) {
			const auto& effects = modes[profile.scalingMode].effects;
			for (size_t i = 0; i < effects.size(); ++i) {
				if ((error == ScalingError::ScalingModeUnknownEffect &&
					(effects[i].isRecoveryInvalid || !EffectsService::Get().GetEffect(effects[i].name))) ||
					(error == ScalingError::ConflictingFrameGenerationEffects &&
						ClassifyFrameGenerationEffect(effects[i].name) != FrameGenerationEffectKind::None)) {
					if (!context.empty()) context += '\n';
					context += fmt::format("#{} {}", i + 1, StrHelper::UTF16ToUTF8(effects[i].name));
				}
			}
		}
		ErrorService::Get().Report(error, std::move(context), hWnd, 0, MakeIssueContext(hWnd, profile));
	}
}

ScalingError ScalingService::_StartScaleImpl(HWND hWnd, const Profile& profile, bool windowedMode, bool force,
	SIZE regionOverlayInternalSize) {
	// ScalingRuntime::Start 会检查是否正在缩放，这里提前检查以避免无效操作
	if (!force && _scalingRuntime->State() == ScalingState::Scaling) {
		return ScalingError::NoError;
	}

	if (WindowHelper::IsForbiddenSystemWindow(hWnd)) {
		return ScalingError::NoError;
	}

	if (profile.scalingMode < 0 || static_cast<uint32_t>(profile.scalingMode) >=
		ScalingModesService::Get().GetScalingModeCount()) {
		return ScalingError::ScalingModeNotSelected;
	}

	const ScalingMode& scalingMode =
		ScalingModesService::Get().GetScalingMode(profile.scalingMode);
	const std::vector<EffectItem>& effects = scalingMode.effects;
	if (effects.empty()) {
		return ScalingError::ScalingModeEmpty;
	} else {
		for (const EffectItem& effect : effects) {
			if (effect.isRecoveryInvalid || !EffectsService::Get().GetEffect(effect.name)) {
				// 存在无法解析的效果
				return ScalingError::ScalingModeUnknownEffect;
			}
		}
	}

	const FrameGenerationChainValidation frameGeneration =
		ValidateFrameGenerationChain(effects);
	if (frameGeneration.HasConflict()) {
		Logger::Get().Error(fmt::format(
			"Scaling mode '{}' contains {} frame-generation effects",
			StrHelper::UTF16ToUTF8(scalingMode.name), frameGeneration.count));
		return ScalingError::ConflictingFrameGenerationEffects;
	}

	if (profile.Is3DGameMode() && windowedMode) {
		return ScalingError::Windowed3DGameMode;
	}
	if (windowedMode && profile.captureMethod == CaptureMethod::DesktopDuplication) {
		return ScalingError::WindowedDesktopDuplication;
	}

	ScalingOptions options;
	options.scalingModeIdx = static_cast<uint32_t>(profile.scalingMode);
	options.scalingModeName = scalingMode.name;

	options.effects.reserve(effects.size());
	for (const EffectItem& effectItem : effects) {
		options.effects.push_back((EffectOption)effectItem);
	}

	// 尝试启用触控支持
	bool isTouchSupportEnabled;
	if (!TouchHelper::TryLaunchTouchHelper(isTouchSupportEnabled)) {
		Logger::Get().Error("TryLaunchTouchHelper 失败");
		return ScalingError::TouchSupport;
	}

	options.graphicsCardId = profile.graphicsCardId;
	options.captureMethod = profile.captureMethod;
	if (profile.isFrameRateLimiterEnabled) {
		options.maxFrameRate = profile.maxFrameRate;
	}
	options.multiMonitorUsage = profile.multiMonitorUsage;
	options.preferredMonitorId = profile.preferredMonitorId;
	options.destAlignment = profile.destAlignment;
	options.cursorInterpolationMode = profile.cursorInterpolationMode;
	options.flags = profile.scalingFlags;
	if (regionOverlayInternalSize.cx > 0 && regionOverlayInternalSize.cy > 0) {
		options.IsRegionOverlayMode(true);
		options.regionOverlayInternalSize = regionOverlayInternalSize;
		options.browserFrameMapping = _browserFrameMapping;
		options.browserFrameSize = _browserFrameSize;
		options.browserUiRects = _browserUiRects;
		for (EffectOption& effect : options.effects) {
			if (effect.name == "RTXVideo\\RTXVideo_VSR") {
				effect.scalingType = ScalingType::Absolute;
				effect.scale = {
					static_cast<float>(regionOverlayInternalSize.cx),
					static_cast<float>(regionOverlayInternalSize.cy)
				};
			}
		}
	}

	options.IsWindowedMode(windowedMode);
	options.IsTouchSupportEnabled(isTouchSupportEnabled);

	switch (profile.initialWindowedScaleFactor) {
	case InitialWindowedScaleFactor::Auto:
		options.initialWindowedScaleFactor = 0.0f;
		break;
	case InitialWindowedScaleFactor::x1:
		options.initialWindowedScaleFactor = 1.0f;
		break;
	case InitialWindowedScaleFactor::x1_25:
		options.initialWindowedScaleFactor = 1.25f;
		break;
	case InitialWindowedScaleFactor::x1_5:
		options.initialWindowedScaleFactor = 1.5f;
		break;
	case InitialWindowedScaleFactor::x1_75:
		options.initialWindowedScaleFactor = 1.75f;
		break;
	case InitialWindowedScaleFactor::x2:
		options.initialWindowedScaleFactor = 2.0f;
		break;
	case InitialWindowedScaleFactor::x3:
		options.initialWindowedScaleFactor = 3.0f;
		break;
	case InitialWindowedScaleFactor::Custom:
		options.initialWindowedScaleFactor = profile.customInitialWindowedScaleFactor;
		break;
	default:
		options.initialWindowedScaleFactor = 0.0f;
		break;
	}

	if (profile.isCroppingEnabled) {
		options.cropping = profile.cropping;
	}

	switch (profile.cursorScaling) {
	case CursorScaling::x0_5:
		options.cursorScaling = 0.5f;
		break;
	case CursorScaling::x0_75:
		options.cursorScaling = 0.75f;
		break;
	case CursorScaling::NoScaling:
		options.cursorScaling = 1.0f;
		break;
	case CursorScaling::x1_25:
		options.cursorScaling = 1.25f;
		break;
	case CursorScaling::x1_5:
		options.cursorScaling = 1.5f;
		break;
	case CursorScaling::x2:
		options.cursorScaling = 2.0f;
		break;
	case CursorScaling::Source:
		// 0 或负值表示和源窗口缩放比例相同
		options.cursorScaling = 0.0f;
		break;
	case CursorScaling::Custom:
		options.cursorScaling = profile.customCursorScaling;
		break;
	default:
		options.cursorScaling = 1.0f;
		break;
	}

	if (profile.isAutoHideCursorEnabled) {
		options.autoHideCursorDelay = profile.autoHideCursorDelay;
	}

	options.isParameterFocusSwitchingEnabled = profile.isParameterFocusSwitchingEnabled;

	// 应用全局配置
	AppSettings& settings = AppSettings::Get();
	options.toolbarShortcutLabels = GetToolbarShortcutLabels();
	options.IsDeveloperMode(settings.IsDeveloperMode());
	options.IsDebugMode(settings.IsDebugMode());
	options.IsBenchmarkMode(settings.IsBenchmarkMode());
	options.IsTopmostDisabled(settings.IsTopmostDisabled());
	options.IsEffectCacheDisabled(settings.IsEffectCacheDisabled());
	options.IsFontCacheDisabled(settings.IsFontCacheDisabled());
	options.IsSaveEffectSources(settings.IsSaveEffectSources());
	options.IsWarningsAreErrors(settings.IsWarningsAreErrors());
	options.IsAllowScalingMaximized(settings.IsAllowScalingMaximized());
	options.IsSimulateExclusiveFullscreen(settings.IsSimulateExclusiveFullscreen());
	options.duplicateFrameDetectionMode = settings.DuplicateFrameDetectionMode();
	options.IsStatisticsForDynamicDetectionEnabled(settings.IsStatisticsForDynamicDetectionEnabled());
	options.IsInlineParams(settings.IsInlineParams());
	options.IsFP16Disabled(settings.IsFP16Disabled());
	options.isFrontEdgeSyncEnabled = settings.IsFrontEdgeSyncEnabled();
	// VRR is deferred while its settings card is hidden. Ignore an older
	// saved true value so no session silently enables tearing.
	options.isVRREnabled = false;
	options.frontEdgeSyncFrameRate = settings.FrontEdgeSyncFrameRate();
	options.frameSyncMode = settings.GetFrameSyncMode();

	if (options.maxFrameRate) {
		// 最小帧数不能大于最大帧数
		options.minFrameRate = std::min(settings.MinFrameRate(), *options.maxFrameRate);
	} else {
		options.minFrameRate = settings.MinFrameRate();
	}

	options.fullscreenInitialToolbarState = settings.FullscreenInitialToolbarState();
	options.windowedInitialToolbarState = settings.WindowedInitialToolbarState();
	if (options.IsRegionOverlayMode()) {
		options.fullscreenInitialToolbarState = ToolbarState::Off;
	}
	options.screenshotsDir = settings.ScreenshotsDir();
	if (options.screenshotsDir.empty()) {
		// 回落到使用当前目录
		options.screenshotsDir = L".";
	}

	options.overlayOptions = settings.OverlayOptions();

	options.showToast = [](HWND hwndTarget, std::wstring_view msg) noexcept {
		ToastService::Get().ShowMessageOnWindow({}, msg, hwndTarget);
	};

	const IssueContext issueContext = MakeIssueContext(hWnd, profile);
	options.showError = [issueContext](HWND target, ScalingError error) noexcept {
		ErrorService::Get().Report(error, {}, target, 0, issueContext);
	};
	options.reportErrorDetails = [issueContext](HWND target, ScalingError error, std::string_view context, uint32_t systemError) noexcept {
		ErrorService::Get().Report(error, std::string(context), target, systemError, issueContext);
	};

	options.save = [](const ScalingOptions& options, HWND /*hwndScaling*/) noexcept {
		App::Get().Dispatcher().TryEnqueue(
			[overlayOptions(options.overlayOptions)]() {
				AppSettings::Get().OverlayOptions() = std::move(overlayOptions);
				AppSettings::Get().SaveAsync();
			}
		);
	};
	options.revertEffectParameter = [modeIdx = options.scalingModeIdx, modeName = options.scalingModeName](
		uint32_t effectIdx, const EffectOption& previousEffect, const std::string& parameter, float rejected, float previous) {
		App::Get().Dispatcher().TryEnqueue([modeIdx, modeName, effectIdx, previousEffect, parameter, rejected, previous] {
			auto& modes = AppSettings::Get().ScalingModes();
			if (modeIdx >= modes.size() || modes[modeIdx].name != modeName || effectIdx >= modes[modeIdx].effects.size()) return;
			auto& effect = modes[modeIdx].effects[effectIdx];
			if (StrHelper::UTF16ToUTF8(effect.name) != previousEffect.name ||
				effect.scale != previousEffect.scale || effect.scalingType != previousEffect.scalingType) return;
			// A later settings/toolbar edit wins over this older failed request.
			if (!RestoreRejectedEffectParameter(effect.parameters, StrHelper::UTF8ToUTF16(parameter), rejected, previous)) return;
			ScalingModesService::Get().EffectParametersChanged.Invoke(modeIdx, effectIdx);
			AppSettings::Get().SaveAsync();
		});
	};

	options.requestEffectParameters = [](
		const ScalingOptions& sessionOptions,
		EffectParametersRequest&& request
	) noexcept -> bool {
		// Do not discard edits while a previous request is queued. Each request
		// owns its snapshot; disk writes are coalesced on the UI thread.
		try {
			return App::Get().Dispatcher().TryEnqueue([
				sessionOptions = ScalingOptions(sessionOptions), request = std::move(request)
			]() mutable {
				const auto state = request.saveState;
				const uint64_t revision = request.revision;
				try {
					ScalingService::Get()._HandleEffectParametersRequest(
						std::move(sessionOptions), std::move(request));
				} catch (...) {
					Logger::Get().Error("Effect parameter request failed with an exception");
					state->Complete(revision, EffectParametersSaveError::WriteFailed);
				}
			});
		} catch (...) {
			Logger::Get().Error("Unable to enqueue effect parameter changes");
			return false;
		}
	};

	if (!_scalingRuntime->Start(hWnd, std::move(options), force)) {
		return ScalingError::ScalingFailedGeneral;
	}

	_hwndCurSrc = hWnd;
	_isCurBrowserVideoScale = regionOverlayInternalSize.cx > 0 && regionOverlayInternalSize.cy > 0;
	if (!force) _isAutoScaleSuspended = false;
	return ScalingError::NoError;
}

void ScalingService::_ScheduleEffectParametersSave(const EffectParametersRequest& request) {
	if (!_effectParametersSaveTimer) {
		_effectParametersSaveTimer = App::Get().Dispatcher().CreateTimer();
		_effectParametersSaveTimer.IsRepeating(false);
		_effectParametersSaveTimer.Tick([this](auto const&, auto const&) {
			_FlushEffectParametersSaves();
		});
	}
	const auto now = std::chrono::steady_clock::now();
	if (_pendingEffectParametersSaves.empty()) _firstEffectParametersEdit = now;
	auto existing = std::ranges::find(_pendingEffectParametersSaves,
		request.saveState, &PendingEffectParametersSave::state);
	if (existing == _pendingEffectParametersSaves.end()) {
		_pendingEffectParametersSaves.push_back({ request.saveState, request.revision });
	} else {
		existing->revision = request.revision;
	}
	// Save after 300 ms of inactivity, and at least once per second during a drag.
	_effectParametersSaveTimer.Stop();
	if (now - _firstEffectParametersEdit >= std::chrono::seconds(1)) {
		_FlushEffectParametersSaves();
	} else {
		_effectParametersSaveTimer.Interval(std::chrono::milliseconds(300));
		_effectParametersSaveTimer.Start();
	}
}

void ScalingService::EffectParameterEdited(uint32_t modeIdx, uint32_t effectIdx,
	const std::string& parameter, float value) {
	if (!_scalingRuntime || (!IsScaling() && _scalingRuntime->State() != ScalingState::Starting)) return;
	const auto& modes = AppSettings::Get().ScalingModes();
	if (modeIdx >= modes.size() || effectIdx >= modes[modeIdx].effects.size()) return;
	_scalingRuntime->UpdateEffectParameterFromSettings(modeIdx, modes[modeIdx].name,
		effectIdx, static_cast<EffectOption>(modes[modeIdx].effects[effectIdx]), parameter, value);
}

void ScalingService::RetryConfigurationSave(std::function<void(bool)> completed) {
	// Capture only failed revisions. A newer edit/conflict must keep its own result.
	std::vector<std::pair<std::shared_ptr<EffectParametersSaveState>, uint64_t>> failed;
	for (const auto& weak : _parameterSaveStates) {
		if (auto state = weak.lock()) {
			const auto result = state->result.load(std::memory_order_acquire);
			if ((result & 7) == static_cast<uint64_t>(EffectParametersSaveError::WriteFailed))
				failed.emplace_back(std::move(state), result);
		}
	}
	AppSettings::Get().SaveAsync([failed = std::move(failed), completed = std::move(completed)](bool succeeded) {
		if (succeeded) {
			for (const auto& [state, result] : failed) {
				auto expected = result;
				state->result.compare_exchange_strong(expected, result & ~uint64_t(7), std::memory_order_acq_rel);
			}
		}
		if (completed) completed(succeeded);
	});
}

void ScalingService::_FlushEffectParametersSaves(bool synchronous) {
	if (_effectParametersSaveTimer) _effectParametersSaveTimer.Stop();
	if (_pendingEffectParametersSaves.empty()) return;
	auto pending = std::exchange(_pendingEffectParametersSaves, {});
	auto complete = [pending = std::move(pending)](bool succeeded) noexcept {
		for (const auto& item : pending) {
			item.state->Complete(item.revision, succeeded ?
				EffectParametersSaveError::None : EffectParametersSaveError::WriteFailed);
		}
	};
	if (synchronous) {
		complete(AppSettings::Get().Save());
	} else {
		AppSettings::Get().SaveAsync(std::move(complete));
	}
}

void ScalingService::_HandleEffectParametersRequest(
	ScalingOptions&& sessionOptions,
	EffectParametersRequest&& request
) {
	std::erase_if(_parameterSaveStates, [](const auto& state) { return state.expired(); });
	if (std::ranges::none_of(_parameterSaveStates, [&](const auto& state) { return state.lock() == request.saveState; }))
		_parameterSaveStates.push_back(request.saveState);
	auto fail = [&](EffectParametersSaveError error) noexcept {
		request.saveState->Complete(request.revision, error);
		if (error == EffectParametersSaveError::Conflict) {
			ErrorService::Get().Report(ScalingError::EffectParameterConflict,
				StrHelper::UTF16ToUTF8(sessionOptions.scalingModeName), request.hwndScaling);
		}
	};
	const bool restart = request.kind == EffectParametersRequestKind::SaveAndRestart;
	if ((request.saveState->result.load(std::memory_order_acquire) & 7) ==
		static_cast<uint64_t>(EffectParametersSaveError::Conflict)) {
		fail(EffectParametersSaveError::Conflict);
		return;
	}
	// Persist already submitted edits even if scaling has just stopped. Only
	// restarting needs live HWNDs and the same scaling run.
	if (restart) {
		if (!_scalingRuntime || _scalingRuntime->State() != ScalingState::Scaling ||
			request.scalingRunId != _scalingRuntime->RunId()) {
			fail(EffectParametersSaveError::SessionExpired);
			return;
		}
		if (!IsWindow(request.hwndSource) || !IsWindow(request.hwndScaling) ||
			request.hwndSource != _hwndCurSrc) {
			fail(EffectParametersSaveError::SourceUnavailable);
			return;
		}
	}

	auto& settings = AppSettings::Get();
	FrameSyncSettings mergedFrameSync{ settings.IsFrontEdgeSyncEnabled(), settings.FrontEdgeSyncFrameRate(), settings.GetFrameSyncMode() };
	if (!MergeFrameSyncSettings(mergedFrameSync, request.previousFrameSync, request.frameSync)) {
		fail(EffectParametersSaveError::Conflict);
		return;
	}
	std::vector<ScalingMode>& modes = settings.ScalingModes();
	if (sessionOptions.scalingModeIdx >= modes.size()) {
		fail(EffectParametersSaveError::Conflict);
		return;
	}
	ScalingMode& mode = modes[sessionOptions.scalingModeIdx];
	if (mode.name != sessionOptions.scalingModeName ||
		mode.effects.size() != sessionOptions.effects.size() ||
		request.effects.size() != mode.effects.size() ||
		request.previousEffects.size() != mode.effects.size()) {
		fail(EffectParametersSaveError::Conflict);
		return;
	}

	auto merged = mode.effects;
	for (size_t i = 0; i < merged.size(); ++i) {
		EffectItem& destination = merged[i];
		const EffectOption& session = sessionOptions.effects[i];
		if (StrHelper::UTF16ToUTF8(destination.name) != session.name ||
			request.effects[i].name != session.name ||
			request.previousEffects[i].name != session.name ||
			destination.scalingType != session.scalingType || destination.scale != session.scale) {
			fail(EffectParametersSaveError::Conflict);
			return;
		}
		phmap::flat_hash_map<std::wstring, float> before, after;
		for (const auto& [name, value] : request.previousEffects[i].parameters) {
			before[StrHelper::UTF8ToUTF16(name)] = value;
		}
		for (const auto& [name, value] : request.effects[i].parameters) {
			after[StrHelper::UTF8ToUTF16(name)] = value;
		}
		if (!MergeEffectParameterChanges(destination.parameters, before, after)) {
			fail(EffectParametersSaveError::Conflict);
			return;
		}
	}
	mode.effects = std::move(merged);
	settings.IsFrontEdgeSyncEnabled(mergedFrameSync.enabled);
	settings.FrontEdgeSyncFrameRate(mergedFrameSync.frameRate);
	settings.SetFrameSyncMode(mergedFrameSync.mode);
	if (sessionOptions.parameterSession) sessionOptions.parameterSession->DesiredFrameSync(mergedFrameSync);
	for (uint32_t i = 0; i < mode.effects.size(); ++i) {
		ScalingModesService::Get().EffectParametersChanged.Invoke(sessionOptions.scalingModeIdx, i);
	}
	if (!restart) {
		_ScheduleEffectParametersSave(request);
		return;
	}

	// Keep the edited settings on failure so the next edit can retry. Never
	// announce success through a throwing ResourceLoader/Toast callback.
	_FlushEffectParametersSaves(true);
	if (!AppSettings::Get().Save()) {
		fail(EffectParametersSaveError::WriteFailed);
		return;
	}
	request.saveState->Complete(request.revision, EffectParametersSaveError::None);
	std::vector<EffectOption> effects;
	for (const EffectItem& item : mode.effects) effects.push_back(static_cast<EffectOption>(item));
	if (!_scalingRuntime->RestartWithEffectParameters(request.hwndSource,
		request.hwndScaling, request.scalingRunId, std::move(effects), mergedFrameSync)) {
		fail(EffectParametersSaveError::SessionExpired);
	}
}

}
