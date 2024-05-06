/*
 * EPMP
 * See: docs/LICENSE-EPMP.txt
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

#ifndef EP_MULTIPLAYER_CHATUI_H
#define EP_MULTIPLAYER_CHATUI_H

#include <string>

class ChatUi {
public:
	static ChatUi& Instance();

	void Refresh(); // initializes chat or refreshes its theme
	void Update(); // called once per logical frame
	void SetFocus(bool focused);

	void GotMessage(int visibility, int room_id, std::string name,
			std::string message, std::string sys_name);

	void GotInfo(std::string msg);
	void SetStatusConnection(bool connected, bool connecting = false);
	void SetStatusRoom(unsigned int room_id);
};

inline ChatUi& CUI() { return ChatUi::Instance(); }

#endif
