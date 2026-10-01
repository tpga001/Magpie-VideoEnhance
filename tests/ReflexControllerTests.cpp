#include "ReflexController.h"
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Magpie;
using namespace std::chrono_literals;

static void Require(bool value, const char* message) {
	if (!value) throw std::runtime_error(message);
}

struct Event {
	std::string name;
	uint64_t frame = 0;
	uint64_t present = 0;
};

class FakeDriver final : public ReflexDriver {
public:
	std::mutex mutex;
	std::vector<Event> events;
	std::vector<ReflexSettings> settings;
	std::string failure;
	bool actualOn = true;
	bool failClear = false;
	bool failClearQuery = false;
	std::function<void(ReflexSettings)> configureHook;
	std::function<void()> sleepHook;
	std::function<void()> presentHook;
	int Record(std::string name, uint64_t frame = 0, uint64_t present = 0) noexcept {
		std::scoped_lock lock(mutex);
		events.push_back({ name, frame, present });
		return failure == name ? -1 : 0;
	}
	ReflexConfigurationResult Configure(ReflexSettings value) noexcept override {
		if (configureHook) configureHook(value);
		{ std::scoped_lock lock(mutex); settings.push_back(value); }
		const int recorded = Record(value.lowLatency ? "on" : "off", value.minimumIntervalUs);
		const int status = failClear && !value.minimumIntervalUs && !value.lowLatency ? -3 : recorded;
		return { status, (failure == "query" && value.lowLatency) ||
			(failClearQuery && !value.lowLatency) ? -2 : 0,
			status == 0, value.lowLatency && actualOn };
	}
	int Sleep() noexcept override {
		if (sleepHook) sleepHook();
		return Record("sleep");
	}
	int Marker(ReflexMarker marker, uint64_t frame) noexcept override {
		static constexpr const char* names[]{ "sim-start", "sim-end", "render-start", "render-end" };
		return Record(names[static_cast<int>(marker)], frame);
	}
	int RegisterGenerationQueue(ID3D12CommandQueue*) noexcept override { return Record("queue"); }
	int Generation(ID3D12CommandQueue*, uint64_t frame, uint64_t present, bool start) noexcept override {
		return Record(start ? "fg-start" : "fg-end", frame, present);
	}
	int FrontendRender(uint64_t frame, uint64_t present, bool start) noexcept override {
		return Record(start ? "front-start" : "front-end", frame, present);
	}
	int Present(uint64_t frame, uint64_t present, bool generated, bool start) noexcept override {
		if (presentHook) presentHook();
		return Record(generated ? (start ? "generated-start" : "generated-end") :
			(start ? "real-start" : "real-end"), frame, present);
	}
	void ReportFailure(const char*, int) noexcept override { Record("failure"); }
	size_t Count(const std::string& name) {
		std::scoped_lock lock(mutex);
		return std::count_if(events.begin(), events.end(), [&](const Event& event) { return event.name == name; });
	}
};

static void Present(ReflexController& reflex, uint64_t frame, uint64_t present, bool generated) {
	reflex.FrontendRender(frame, present, true);
	reflex.FrontendRender(frame, present, false);
	reflex.Present(frame, present, generated, true);
	reflex.Present(frame, present, generated, false);
}

static void TestFrameLifecycle() {
	for (uint32_t multiplier = 2; multiplier <= 4; ++multiplier) {
		ReflexController reflex;
		auto fake = std::make_unique<FakeDriver>();
		auto* driver = fake.get();
		reflex.Initialize(std::move(fake));
		reflex.RegisterGenerationQueue(nullptr);
		const auto frame = reflex.BeginCapture(20);
		Require(frame == 20, "Reflex must align with the minimum NGX base-frame ID");
		for (int retry = 0; retry < 50; ++retry)
			Require(reflex.BeginCapture(20) == frame, "duplicate capture must retain its ID");
		reflex.EndCaptureRender();
		reflex.EndCaptureRender();
		uint64_t priorId = 0;
		for (uint32_t index = 1; index < multiplier; ++index) {
			const auto presentId = reflex.NextPresentId();
			Require(presentId > priorId, "generated IDs must be unique and ordered");
			priorId = presentId;
			reflex.Generation(nullptr, frame, presentId, true);
			reflex.Generation(nullptr, frame, presentId, false);
			// Disabled/reset interpolation may consume an ID without presenting.
			if (index != 2) Present(reflex, frame, presentId, true);
		}
		const auto realPresent = reflex.NextPresentId();
		Require(realPresent > priorId, "real image must follow interpolation IDs");
		reflex.CompleteCapture();
		Require(driver->Count("sleep") == 1, "one Sleep per base input, not per generated frame or retry");
		Require(driver->Count("render-end") == 1, "render end must not be duplicated");
		// Backend runs ahead: frontend must retain the previous published IDs.
		const auto nextFrame = reflex.BeginCapture(21);
		Require(nextFrame == 21, "next frame increments once");
		Present(reflex, frame, realPresent, false);
		Require(driver->events.back().frame == frame && driver->events.back().present == realPresent,
			"Present must use the published frame, not the newest backend frame");
		reflex.CompleteCapture();
		const auto beforeOverlay = driver->events.size();
		Present(reflex, 0, 0, false);
		Require(driver->events.size() == beforeOverlay, "overlay-only redraw must not invent a base input");
		Require(driver->events[1].name == "queue" && driver->events[2].name == "sleep" &&
			driver->events[3].name == "sim-start" && driver->events[4].name == "sim-end" &&
			driver->events[5].name == "render-start", "Sleep and base markers must follow the NVAPI contract");
		reflex.Stop();
		reflex.Stop();
		Require(driver->Count("on") == 1 && driver->Count("off") == 1, "configure only on transitions");
	}
}

static void TestPauseAndFailure() {
	ReflexController absent;
	absent.Initialize(nullptr);
	Require(absent.BeginCapture() == 0 && !absent.Available(), "unsupported path must stay inactive");
	for (const auto* failure : { "on", "query", "sleep", "sim-start", "sim-end", "render-start", "render-end",
		"queue", "fg-start", "fg-end", "front-start", "front-end", "generated-start", "real-end" }) {
		ReflexController reflex;
		auto fake = std::make_unique<FakeDriver>();
		auto* driver = fake.get();
		driver->failure = failure;
		reflex.Initialize(std::move(fake));
		reflex.RegisterGenerationQueue(nullptr);
		const auto frame = reflex.BeginCapture();
		const auto present = reflex.NextPresentId();
		reflex.EndCaptureRender();
		reflex.Generation(nullptr, frame, present, true);
		reflex.Generation(nullptr, frame, present, false);
		Present(reflex, frame, present, true);
		Present(reflex, frame, present, false);
		reflex.CompleteCapture();
		Require(!reflex.Available(), "driver error must disable Reflex");
		Require(driver->Count("failure") == 1 && driver->Count("off") == 1,
			"failure must log once and restore Off once");
		const auto stoppedEvents = driver->events.size();
		reflex.SetPresentationAvailable(true);
		Require(reflex.BeginCapture() == 0, "faulted session must not repeatedly retry the driver");
		Present(reflex, frame, present, false);
		Require(driver->events.size() == stoppedEvents, "fallback must stop all subsequent driver calls");
	}
	ReflexController reflex;
	auto fake = std::make_unique<FakeDriver>();
	auto* driver = fake.get();
	reflex.Initialize(std::move(fake));
	const auto cancelled = reflex.BeginCapture();
	reflex.CompleteCapture();
	reflex.SetPresentationAvailable(false);
	for (int retry = 0; retry < 20; ++retry) {
		reflex.SetPresentationAvailable(false);
		Require(reflex.BeginCapture() == 0, "DComp must not use the DXGI Reflex sleeper");
	}
	reflex.SetPresentationAvailable(true);
	Require(reflex.BeginCapture(42) > cancelled && reflex.CaptureFrameId() == 42,
		"resize/resume must preserve monotonic IDs and synchronize with NGX");
	reflex.CompleteCapture();
	Require(driver->Count("sleep") == 2 && driver->Count("on") == 2 && driver->Count("off") == 1,
		"pause/resume must configure once without repeated polling sleeps");
}

static void TestSleepDoesNotBlockPresentOrStop() {
	ReflexController reflex;
	auto fake = std::make_unique<FakeDriver>();
	auto* driver = fake.get();
	reflex.Initialize(std::move(fake));
	const auto first = reflex.BeginCapture();
	const auto present = reflex.NextPresentId();
	reflex.CompleteCapture();
	std::promise<void> sleepEntered, releaseSleep;
	auto entered = sleepEntered.get_future();
	auto release = releaseSleep.get_future().share();
	driver->sleepHook = [&] { sleepEntered.set_value(); release.wait_for(2s); };
	auto backend = std::async(std::launch::async, [&] { reflex.BeginCapture(); reflex.CompleteCapture(); });
	Require(entered.wait_for(1s) == std::future_status::ready, "backend did not enter Sleep");
	auto frontend = std::async(std::launch::async, [&] {
		Present(reflex, first, present, false);
		reflex.Stop();
	});
	const bool frontendFinished = frontend.wait_for(1s) == std::future_status::ready;
	releaseSleep.set_value();
	backend.get();
	frontend.get();
	Require(frontendFinished, "Sleep must not hold a mutex needed by Present or shutdown");
	Require(driver->Count("real-end") == 1 && driver->Count("off") == 1 && !reflex.Available(),
		"concurrent shutdown must preserve Present and restore driver state");
}

static void TestOrdinaryFrameLimit() {
	ReflexController reflex;
	auto fake = std::make_unique<FakeDriver>();
	auto* driver = fake.get();
	reflex.Initialize(std::move(fake), { .minimumIntervalUs = 12500 });
	Require(driver->settings.back() == ReflexSettings{ .minimumIntervalUs = 12500 }, "80 FPS and Boost Off must reach the driver");
	for (int i = 0; i < 30; ++i) reflex.SetFrameRateLimit(12500);
	Require(driver->settings.size() == 1, "unchanged target must not configure every frame");
	const auto frame = reflex.BeginCapture(9);
	const auto present = reflex.NextPresentId();
	for (int i = 0; i < 20; ++i) reflex.BeginCapture(9);
	reflex.EndCaptureRender();
	reflex.CompleteCapture();
	Present(reflex, frame, present, false);
	Require(driver->Count("sleep") == 1 && driver->Count("real-end") == 1 && driver->Count("queue") == 0,
		"ordinary rendering needs one sleep and no FG queue");
	reflex.SetPresentationAvailable(false);
	reflex.SetFrameRateLimit(10000);
	Require(driver->settings.back() == ReflexSettings{ .lowLatency = false }, "pause must clear the cap and low latency");
	reflex.SetPresentationAvailable(true);
	Require(driver->settings.back() == ReflexSettings{ .minimumIntervalUs = 10000 }, "resume must restore the latest target");
	reflex.SetFrameRateLimit(0);
	Require(driver->settings.back().lowLatency && !driver->settings.back().boost && driver->settings.back().minimumIntervalUs == 0,
		"clearing the frame limit must preserve low latency");
	driver->failure = "on";
	reflex.SetFrameRateLimit(16667);
	Require(!reflex.Available() && driver->settings.back() == ReflexSettings{ .lowLatency = false },
		"failed reconfiguration must clear all driver settings before Async fallback");
}

static void TestDriverOffIsNotFailure() {
	for (uint32_t interval : { 0u, 16667u }) {
		ReflexController reflex;
		auto fake = std::make_unique<FakeDriver>();
		auto* driver = fake.get();
		driver->actualOn = false;
		reflex.Initialize(std::move(fake), { .minimumIntervalUs = interval });
		Require(reflex.State() == ReflexState::DriverOff && reflex.CanResume() && !reflex.Available(),
			"successful Off query must remain distinct from unavailable or driver failure");
		for (int i = 0; i < 100; ++i) {
			reflex.SetPresentationAvailable(true);
			reflex.SetFrameRateLimit(interval);
			Require(reflex.BeginCapture() != 0, "usable driver must keep Sleep/markers even with low latency Off");
		}
		Require(driver->Count("failure") == 0 && driver->Count("on") == 1 && driver->Count("sleep") == 1,
			"Off query must not synthesize a failure or repeatedly force driver activation");
		Require(driver->settings.back().minimumIntervalUs == interval && reflex.PacingAvailable() == (interval != 0),
			"driver Off query must preserve independent accepted interval");
		reflex.SetPresentationAvailable(false);
		Require(reflex.State() == ReflexState::Paused, "presentation pause must differ from driver Off");
		driver->actualOn = true;
		reflex.SetPresentationAvailable(true);
		Require(reflex.State() == ReflexState::Active && driver->settings.back().minimumIntervalUs == interval,
			"presentation recovery can re-query and restore the requested limit");
		reflex.Stop();
		Require(reflex.State() == ReflexState::Stopped, "explicit stop must differ from driver failure");
	}
}

static void TestClearBeforeFallback() {
	for (bool failClear : {false, true}) {
		ReflexController reflex;
		auto fake = std::make_unique<FakeDriver>();
		auto* driver = fake.get();
		reflex.Initialize(std::move(fake), { .minimumIntervalUs = 16667 });
		driver->failClear = failClear;
		driver->configureHook = [&](ReflexSettings settings) {
			if (!settings.minimumIntervalUs) {
				Require(!reflex.CanUseAsync() && reflex.CaptureBlocked(), "Async must wait for clear completion");
			}
		};
		driver->failure = "sleep";
		Require(reflex.BeginCapture() == 0, "failed sleep must not accept input");
		Require(reflex.CanUseAsync() == !failClear, "clear failure must never claim safe Async fallback");
		Require(reflex.CaptureBlocked() == failClear, "clear failure must block capture until session stops");
		const auto calls = driver->events.size();
		for (int retry = 0; retry < 20; ++retry) {
			reflex.Stop(); reflex.SetFrameRateLimit(10000); reflex.SetPresentationAvailable(true);
			Require(!reflex.BeginCapture(), "faulted candidate must stay closed");
		}
		Require(calls == driver->events.size(), "failed cleanup must not cause retry/log storms");
	}
}

static void TestCandidateAcrossPause() {
	ReflexController reflex;
	reflex.Initialize(std::make_unique<FakeDriver>(), { .minimumIntervalUs = 10000 });
	const auto old = reflex.BeginCapture(70);
	reflex.SetPresentationAvailable(false);
	Require(!reflex.BeginCapture() && reflex.CanUseAsync(), "paused driver must clear before fallback");
	reflex.SetFrameRateLimit(12500);
	reflex.SetPresentationAvailable(true);
	Require(reflex.BeginCapture(71) > old && reflex.PacingAvailable(), "resume must use a new slept candidate");
}

static void TestPauseDuringSleepAndClearQueryFailure() {
	ReflexController reflex;
	auto fake = std::make_unique<FakeDriver>();
	auto* driver = fake.get();
	reflex.Initialize(std::move(fake), { .minimumIntervalUs = 10000 });
	std::promise<void> sleepEntered, releaseSleep;
	auto entered = sleepEntered.get_future();
	auto release = releaseSleep.get_future().share();
	driver->sleepHook = [&] { sleepEntered.set_value(); release.wait_for(2s); };
	auto backend = std::async(std::launch::async, [&] { return reflex.BeginCapture(80); });
	Require(entered.wait_for(1s) == std::future_status::ready, "capture did not enter Sleep");
	reflex.SetPresentationAvailable(false);
	reflex.SetFrameRateLimit(12500);
	reflex.SetPresentationAvailable(true);
	releaseSleep.set_value();
	Require(backend.get() == 0, "Sleep spanning configuration changes must not accept a stale candidate");
	driver->sleepHook = {};
	Require(reflex.BeginCapture(80) == 80 && driver->Count("sleep") == 2,
		"new configuration needs a fresh Sleep before capture");
	reflex.CompleteCapture();
	driver->failClearQuery = true;
	reflex.Stop();
	Require(reflex.CanUseAsync() && !reflex.CaptureBlocked() && driver->settings.back().minimumIntervalUs == 0,
		"successful clear Set must permit fallback even if low-latency query fails");
	Require(driver->Count("failure") == 1, "clear query failure must still be reported");
}

static void TestUnsupportedZeroCapCanFallback() {
	ReflexController reflex;
	auto fake = std::make_unique<FakeDriver>();
	auto* driver = fake.get();
	driver->failure = "on";
	driver->failClear = true;
	reflex.Initialize(std::move(fake));
	Require(reflex.State() == ReflexState::Faulted && !reflex.CanResume(), "unsupported API must disable this session");
	Require(reflex.CanUseAsync() && !reflex.CaptureBlocked(), "never-requested application cap cannot block Async fallback");
	Require(std::ranges::all_of(driver->settings, [](ReflexSettings settings) { return settings.minimumIntervalUs == 0; }),
		"this fallback is only safe because no nonzero interval was requested");
}

int main() {
	try {
		TestFrameLifecycle();
		TestPauseAndFailure();
		TestSleepDoesNotBlockPresentOrStop();
		TestOrdinaryFrameLimit();
		TestDriverOffIsNotFailure();
		TestClearBeforeFallback();
		TestCandidateAcrossPause();
		TestPauseDuringSleepAndClearQueryFailure();
		TestUnsupportedZeroCapCanFallback();
		std::cout << "PASS: Reflex 2x/3x/4x IDs, capture retries, skipped interpolation, FIFO IDs, 14 driver failure points, concurrent Sleep/Present/Stop; ordinary frame limits, same-target deduplication, Off-query distinction and pause/resume\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
