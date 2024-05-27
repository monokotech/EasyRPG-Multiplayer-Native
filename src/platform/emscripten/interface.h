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

#ifndef EP_EMSCRIPTEN_INTERFACE_H
#define EP_EMSCRIPTEN_INTERFACE_H

#include <string>

class Emscripten_Interface {
public:
	static bool DownloadSavegame(int slot);
	static void UploadSavegame(int slot);
	static void RefreshScene();
	static void TakeScreenshot();
	static void Reset();

	// IME & Clipboard support
	static void StartTextInput();
	static void StopTextInput();
	static void SetTextInputRect(int x, int y, int w = 0, int h = 0);
	static void UpdateTextInputBuffer(std::string text);
	static std::string GetClipboardText();
	static void SetClipboardText(std::string_view text);
};

class Emscripten_Interface_Private {
public:
	static bool UploadSavegameStep2(int slot, int buffer_addr, int size);
};

#endif
