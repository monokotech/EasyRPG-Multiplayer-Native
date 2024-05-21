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

#ifndef EP_OUTPUT_MT_H
#define EP_OUTPUT_MT_H

#include "../output.h"

/**
 * Multi-threaded Output
 */

namespace OutputMt {
	template <typename FmtStr, typename... Args>
	void Info(FmtStr&& fmtstr, Args&&... args);

	void InfoStr(std::string const& msg);


	template <typename FmtStr, typename... Args>
	void Warning(FmtStr&& fmtstr, Args&&... args);

	void WarningStr(std::string const& warn);


	template <typename FmtStr, typename... Args>
	void Debug(FmtStr&& fmtstr, Args&&... args);

	void DebugStr(std::string const& msg);


	void Update();
}

template <typename FmtStr, typename... Args>
inline void OutputMt::Info(FmtStr&& fmtstr, Args&&... args) {
	InfoStr(fmt::format(std::forward<FmtStr>(fmtstr), std::forward<Args>(args)...));
}

template <typename FmtStr, typename... Args>
inline void OutputMt::Warning(FmtStr&& fmtstr, Args&&... args) {
	WarningStr(fmt::format(std::forward<FmtStr>(fmtstr), std::forward<Args>(args)...));
}

template <typename FmtStr, typename... Args>
inline void OutputMt::Debug(FmtStr&& fmtstr, Args&&... args) {
	DebugStr(fmt::format(std::forward<FmtStr>(fmtstr), std::forward<Args>(args)...));
}

#endif
