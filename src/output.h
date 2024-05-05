/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef EP_OUTPUT_H
#define EP_OUTPUT_H

// Headers
#include <string>
#include <iosfwd>
#include <fmt/core.h>

#ifndef SERVER
#  include "filesystem_stream.h"
#else
#  include "utils.h"
#  include <iostream>
#endif

enum class LogLevel {
	Error,
	Warning,
	Info,
	Debug
};

using LogCallbackUserData = void*;
using LogCallbackFn = void (*)(LogLevel lvl, std::string const& message,
	LogCallbackUserData userdata);

/**
 * Output Namespace.
 */
namespace Output {
#ifndef SERVER
	/** @return the configurated log level */
	LogLevel GetLogLevel();

	/**
	 * Sets the log level for filtering logs
	 *
	 * @param ll the new log level
	 */
	void SetLogLevel(LogLevel lvl);

	/**
	 * Sets terminal log colors
	 *
	 * @param colored whether to color terminal log
	 */
	void SetTermColor(bool colored);

	/**
	 * Closes the log file handle and trims the file.
	 */
	void Quit();

	/**
	 * Takes screenshot and save it in the save directory.
	 *
	 * @return true if success, otherwise false.
	 */
	bool TakeScreenshot();

	/**
	 * Takes screenshot and save it to specified file.
	 *
	 * @param file file to save.
	 * @return true if success, otherwise false.
	 */
	bool TakeScreenshot(StringView file);

	/**
	 * Takes screenshot and save it to specified stream.
	 *
	 * @param os output stream that PNG will be stored.
	 * @return true if success, otherwise false.
	 */
	bool TakeScreenshot(std::ostream& os);

	/**
	 * Shows/Hides the output log overlay.
	 */
	void ToggleLog();

	/**
	 * Ignores pause in Warning and Error.
	 *
	 * @param val whether to ignore pause.
	 */
	void IgnorePause(bool val);

	/**
	 * Outputs debug messages over custom logger. Useful for emulators.
	 *
	 * @param fn custom callback function
	 * @param userdata passed as is to callback
	 */
	void SetLogCallback(LogCallbackFn fn, LogCallbackUserData userdata = nullptr);

	/** @return the Loglevel as string */
	std::string LogLevelToString(LogLevel lvl);

	/**
	 * Displays an info string with formatted string.
	 *
	 * @param fmtstr the format string
	 * @param args formatting arguments
	 */
	template <typename FmtStr, typename... Args>
	void Info(FmtStr&& fmtstr, Args&&... args);

	/**
	 * Displays an info string msg.
	 *
	 * @param msg string to display.
	 */
	void InfoStr(std::string const& msg, bool no_chat = false);

	/**
	 * Display a warning with formatted string.
	 *
	 * @param fmtstr the format string
	 * @param args formatting arguments
	 */
	template <typename FmtStr, typename... Args>
	void Warning(FmtStr&& fmtstr, Args&&... args);

	/**
	 * Display a warning.
	 *
	 * @param warn : warning to display.
	 */
	void WarningStr(std::string const& warn, bool no_chat = false);

	/**
	 * Raises an error message with formatted string and
	 * closes the player afterwards.
	 *
	 * @param fmtstr the format string
	 * @param args formatting arguments
	 */
	template <typename FmtStr, typename... Args>
	[[noreturn]] void Error(FmtStr&& fmtstr, Args&&... args);

	/**
	 * Display an error message and closes the player
	 * afterwards.
	 *
	 * @param err error to display.
	 */
	[[noreturn]] void ErrorStr(std::string const& err);

	/**
	 * Prints a debug message to the console.
	 *
	 * @param fmtstr the format string
	 * @param args formatting arguments
	 */
	template <typename FmtStr, typename... Args>
	void Debug(FmtStr&& fmtstr, Args&&... args);

	/**
	 * Prints a debug message to the console.
	 *
	 * @param msg formatted debug text to display.
	 */
	void DebugStr(std::string const& msg);

	template <typename FmtStr, typename... Args>
	void InfoNoChat(FmtStr&& fmtstr, Args&&... args);

	template <typename FmtStr, typename... Args>
	void WarningNoChat(FmtStr&& fmtstr, Args&&... args);
#else // SERVER
	template <typename FmtStr, typename... Args>
	void Info(FmtStr&& fmtstr, Args&&... args);

	template <typename FmtStr, typename... Args>
	void Warning(FmtStr&& fmtstr, Args&&... args);

	template <typename FmtStr, typename... Args>
	[[noreturn]] void Error(FmtStr&& fmtstr, Args&&... args);

	template <typename FmtStr, typename... Args>
	void Debug(FmtStr&& fmtstr, Args&&... args);
#endif // else SERVER
}

#ifndef SERVER
template <typename FmtStr, typename... Args>
inline void Output::Info(FmtStr&& fmtstr, Args&&... args) {
	InfoStr(fmt::format(std::forward<FmtStr>(fmtstr), std::forward<Args>(args)...));
}

template <typename FmtStr, typename... Args>
inline void Output::Error(FmtStr&& fmtstr, Args&&... args) {
	ErrorStr(fmt::format(std::forward<FmtStr>(fmtstr), std::forward<Args>(args)...));
}

template <typename FmtStr, typename... Args>
inline void Output::Warning(FmtStr&& fmtstr, Args&&... args) {
	WarningStr(fmt::format(std::forward<FmtStr>(fmtstr), std::forward<Args>(args)...));
}

template <typename FmtStr, typename... Args>
inline void Output::Debug(FmtStr&& fmtstr, Args&&... args) {
	DebugStr(fmt::format(std::forward<FmtStr>(fmtstr), std::forward<Args>(args)...));
}

template <typename FmtStr, typename... Args>
inline void Output::InfoNoChat(FmtStr&& fmtstr, Args&&... args) {
	InfoStr(fmt::format(std::forward<FmtStr>(fmtstr), std::forward<Args>(args)...), true);
}

template <typename FmtStr, typename... Args>
inline void Output::WarningNoChat(FmtStr&& fmtstr, Args&&... args) {
	WarningStr(fmt::format(std::forward<FmtStr>(fmtstr), std::forward<Args>(args)...), true);
}
#else // SERVER
inline std::string output_time() {
	std::time_t t = std::time(nullptr);
	return Utils::FormatDate(std::localtime(&t), "[%Y-%m-%d %H:%M:%S] ");
}

template <typename FmtStr, typename... Args>
inline void Output::Info(FmtStr&& fmtstr, Args&&... args) {
	std::cout << output_time() << "Info: "
		<< (fmt::format(std::forward<FmtStr>(fmtstr), std::forward<Args>(args)...)) << std::endl;
}

template <typename FmtStr, typename... Args>
inline void Output::Error(FmtStr&& fmtstr, Args&&... args) {
	std::cerr << output_time() << "Error: "
		<< (fmt::format(std::forward<FmtStr>(fmtstr), std::forward<Args>(args)...)) << std::endl;
}

template <typename FmtStr, typename... Args>
inline void Output::Warning(FmtStr&& fmtstr, Args&&... args) {
	std::cout << output_time() << "Warning: "
		<< (fmt::format(std::forward<FmtStr>(fmtstr), std::forward<Args>(args)...)) << std::endl;
}

template <typename FmtStr, typename... Args>
inline void Output::Debug(FmtStr&& fmtstr, Args&&... args) {
	std::cout << output_time() << "Debug: "
		<< (fmt::format(std::forward<FmtStr>(fmtstr), std::forward<Args>(args)...)) << std::endl;
}
#endif // else SERVER

#endif
