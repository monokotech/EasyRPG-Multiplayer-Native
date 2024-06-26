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

#ifndef EP_PLAYEROTHER_H
#define EP_PLAYEROTHER_H

#include <deque>
#include <memory>
#include <map>

struct Game_PlayerOther;
struct Sprite_Character;
struct NameTag;
struct BattleAnimation;

struct PlayerOther {
	std::deque<std::tuple<int8_t, int, int>> mvq; // queue of move commands
	std::unique_ptr<Game_PlayerOther> ch; // character
	std::unique_ptr<Sprite_Character> sprite;
	std::unique_ptr<NameTag> name_tag;
	std::unique_ptr<BattleAnimation> battle_animation; // battle animation

	// type => pos
	std::map<int8_t, std::tuple<int8_t, int16_t, int16_t>> previous_pos;

	// create a copy of this
	// the copied player has no name, no battle animation and no move commands
	// but it is visible, in other words this function modifies the
	// global drawable list
	//
	// the player must be put inside fadeout_players after creation
	// destroying the player outside fadeout_players is undefined behavior
	PlayerOther GetCopy();
};

#endif
