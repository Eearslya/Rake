#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <Luna/Utility/Log.hpp>
#include <filesystem>

namespace Luna {
std::shared_ptr<spdlog::logger> Log::_mainLogger;

void Log::Initialize() {
	const std::filesystem::path logDirectory = "Logs";
	if (!std::filesystem::exists(logDirectory)) { std::filesystem::create_directories(logDirectory); }
	const auto logFile = logDirectory / "Luna.log";

	auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	auto fileSink    = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile.string(), true);

	consoleSink->set_pattern("%^[%T] %n-%L: %v%$");
	fileSink->set_pattern("[%T] %n-%L: %v");

	_mainLogger = std::make_shared<spdlog::logger>("Luna", spdlog::sinks_init_list{consoleSink, fileSink});
	spdlog::register_logger(_mainLogger);

#ifdef LUNA_DEBUG
	SetLevel(Level::Trace);
#endif
}

void Log::Shutdown() {
	_mainLogger.reset();
	spdlog::drop_all();
}
}  // namespace Luna
