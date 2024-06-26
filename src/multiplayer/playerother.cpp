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

#include "playerother.h"
#include "output.h"
#include "scene.h"
#include "drawable_mgr.h"
#include "../sprite_character.h"
#include "../battle_animation.h"
#include "game_playerother.h"
#include "nametag.h"

PlayerOther PlayerOther::GetCopy() {
	PlayerOther po;
	auto scene_map = Scene::Find(Scene::SceneType::Map);
	if (!scene_map) {
		Output::Error("MP: unexpected, {}:{}", __FILE__, __LINE__);
		return po;
	}

	auto old_list = &DrawableMgr::GetLocalList();
	DrawableMgr::SetLocalList(&scene_map->GetDrawableList());

	po.ch.reset(new Game_PlayerOther(0));
	{
		auto& p = *po.ch;
		p.SetX(ch->GetX());
		p.SetY(ch->GetY());
		p.SetFacing(ch->GetFacing());
		p.SetSpriteGraphic(ch->GetSpriteName(), ch->GetSpriteIndex());
		p.SetMoveSpeed(ch->GetMoveSpeed());
		p.SetMoveFrequency(ch->GetMoveFrequency());
		p.SetThrough(ch->GetThrough());
		p.SetLayer(ch->GetLayer());
		p.SetMultiplayerVisible(ch->IsMultiplayerVisible());
		p.SetBaseOpacity(32);
		Color fc = ch->GetFlashColor();
		p.Flash(fc.red / 8, fc.green / 8, fc.blue / 8, ch->GetFlashLevel(), ch->GetFlashTimeLeft());
	}
	po.sprite.reset(new Sprite_Character(po.ch.get()));
	po.sprite->SetTone(sprite->GetTone());
	DrawableMgr::SetLocalList(old_list);
	return po;
}
