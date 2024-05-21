/*
 * EPMP
 * Copyright (C) 2024 Monokotech
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <mutex>
#include "output_mt.h"

#ifndef SERVER
#  include "../player.h"
#endif

namespace {
#ifndef SERVER
	struct Log {
		LogLevel lvl;
		std::string msg;

		Log(LogLevel _lvl, std::string _msg) : lvl(_lvl), msg(std::move(_msg)) {}
	};
	std::vector<Log> log_buffer;
#endif
	std::mutex m_mutex;
}

void OutputMt::WarningStr(std::string const& warn) {
	const std::lock_guard<std::mutex> lock(m_mutex);
#ifndef SERVER
	if (Player::server_flag || Player::is_paused) {
		Output::WarningStr(warn, true);
		return;
	}
	log_buffer.emplace_back(LogLevel::Warning, warn);
#else
	Output::WarningStr(warn);
#endif
}

void OutputMt::InfoStr(std::string const& msg) {
	const std::lock_guard<std::mutex> lock(m_mutex);
#ifndef SERVER
	if (Player::server_flag || Player::is_paused) {
		Output::InfoStr(msg, true);
		return;
	}
	log_buffer.emplace_back(LogLevel::Info, msg);
#else
	Output::InfoStr(msg);
#endif
}

void OutputMt::DebugStr(std::string const& msg) {
	const std::lock_guard<std::mutex> lock(m_mutex);
#ifndef SERVER
	if (Player::server_flag || Player::is_paused) {
		Output::DebugStr(msg);
		return;
	}
	log_buffer.emplace_back(LogLevel::Debug, msg);
#else
	Output::DebugStr(msg);
#endif
}

#ifndef SERVER
void OutputMt::Update() {
	const std::lock_guard<std::mutex> lock(m_mutex);
	if (!log_buffer.empty()) {
		std::vector<Log> local_log_buffer = std::move(log_buffer);
		for (const Log& log : local_log_buffer) {
			switch (log.lvl) {
				case LogLevel::Error:
				case LogLevel::Warning:
					Output::WarningStr(log.msg);
					break;
				case LogLevel::Info:
					Output::InfoStr(log.msg);
					break;
				case LogLevel::Debug:
					Output::DebugStr(log.msg);
					break;
			}
		}
		local_log_buffer.clear();
	}
}
#endif
