#include "pch.h"
#include "Logger.h"
#include "StrHelper.h"
#include <spdlog/sinks/rotating_file_sink.h>

thread_local Logger::DiagnosticCapture* Logger::_diagnosticCapture = nullptr;

Logger::DiagnosticCapture::DiagnosticCapture() noexcept : _previous(_diagnosticCapture) {
	_diagnosticCapture = this;
}

Logger::DiagnosticCapture::~DiagnosticCapture() {
	_diagnosticCapture = _previous;
}

void Logger::_CaptureSystemError(uint32_t error) noexcept {
	for (auto* capture = _diagnosticCapture; capture; capture = capture->_previous) {
		if (!capture->_systemError) capture->_systemError = error;
	}
}

bool Logger::Initialize(spdlog::level::level_enum logLevel, std::wstring logFileName, int logArchiveAboveSize, int logMaxArchiveFiles) noexcept {
	try {
		_logger = spdlog::rotating_logger_mt(".", std::move(logFileName), logArchiveAboveSize, logMaxArchiveFiles);
		_logger->set_level(logLevel);
		_logger->set_pattern("%Y-%m-%d %H:%M:%S.%e|%l|%s:%#|%!|%v");
		_logger->flush_on(spdlog::level::warn);
#ifdef _DEBUG
		spdlog::flush_every(5s);
#else
		spdlog::flush_every(30s);
#endif
	} catch (const spdlog::spdlog_ex&) {
		return false;
	}

	return true;
}

void Logger::SetLevel(spdlog::level::level_enum logLevel) noexcept {
	assert(_logger);

	_logger->flush();
	_logger->set_level(logLevel);

	static const char* LOG_LEVELS[7] = {
		"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "CRITICAL", "OFF"
	};
	Info(fmt::format("当前日志级别: {}", LOG_LEVELS[logLevel]));
}

void Logger::_Log(spdlog::level::level_enum logLevel, std::string_view msg, const SourceLocation& location) noexcept {
	assert(!msg.empty());
	if (logLevel >= spdlog::level::err) {
		try {
			for (auto* capture = _diagnosticCapture; capture; capture = capture->_previous) {
				if (capture->_details.size() >= 4096) continue;
				if (!capture->_details.empty()) capture->_details += '\n';
				capture->_details.append(msg.substr(0, 4096 - capture->_details.size()));
			}
		} catch (...) {
			// Diagnostics must never interfere with the original operation.
		}
	}

	// 只检查一次是否附加了调试器
	static const bool isDebuggerPresent = IsDebuggerPresent();
	if (isDebuggerPresent && logLevel >= spdlog::level::warn) {
		// 警告或更高等级的日志也记录到调试器
		if (msg.back() == '\n') {
			OutputDebugString(StrHelper::Concat(L"[LOG] ", StrHelper::UTF8ToUTF16(msg)).c_str());
		} else {
			OutputDebugString(StrHelper::Concat(L"[LOG] ", StrHelper::UTF8ToUTF16(msg), L"\n").c_str());
		}
	}

	if (_logger) {
		_logger->log(
			spdlog::source_loc{ location.FileName(), (int)location.Line(), location.FunctionName() },
			logLevel,
			msg
		);
	}
}
