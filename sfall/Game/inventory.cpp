/*
 *    sfall
 *    Copyright (C) 2008-2021  The sfall team
 *
 */

#include "..\main.h"
#include "..\FalloutEngine\Fallout2.h"

#include "..\Modules\Inventory.h"
#include "..\Modules\PartyControl.h"

#include "inventory.h"

namespace game
{

namespace sf = sfall;

DWORD __stdcall Inventory::item_total_size(fo::GameObject* critter) {
	int totalSize = fo::func::item_c_curr_size(critter);

	if (critter->TypeFid() == fo::OBJ_TYPE_CRITTER) {
		fo::GameObject* item = fo::func::inven_right_hand(critter);
		if (item && !(item->flags & fo::ObjectFlag::Right_Hand)) {
			totalSize += fo::func::item_size(item);
		}

		fo::GameObject* itemL = fo::func::inven_left_hand(critter);
		if (itemL && item != itemL && !(itemL->flags & fo::ObjectFlag::Left_Hand)) {
			totalSize += fo::func::item_size(itemL);
		}

		item = fo::func::inven_worn(critter);
		if (item && !(item->flags & fo::ObjectFlag::Worn)) {
			totalSize += fo::func::item_size(item);
		}
	}
	return totalSize;
}

// Reimplementation of adjust_fid engine function
// Differences from vanilla:
// - doesn't use art_vault_guy_num as default art, uses current critter FID instead
// - invokes onAdjustFid delegate that allows to hook into FID calculation
DWORD __stdcall Inventory::adjust_fid() {
	DWORD fid;
	if (fo::var::inven_dude->TypeFid() == fo::OBJ_TYPE_CRITTER) {
		DWORD indexNum;
		DWORD weaponAnimCode = 0;
		if (sf::PartyControl::IsNpcControlled()) {
			// if NPC is under control, use current FID of critter
			indexNum = fo::var::inven_dude->artFid & 0xFFF;
		} else {
			// vanilla logic:
			indexNum = fo::var::art_vault_guy_num;
			auto critterPro = fo::GetProto(fo::var::inven_pid);
			if (critterPro != nullptr) {
				indexNum = critterPro->fid & 0xFFF;
			}
			if (fo::var::i_worn != nullptr) {
				auto armorPro = fo::GetProto(fo::var::i_worn->protoId);
				DWORD armorFid = fo::func::stat_level(fo::var::inven_dude, fo::STAT_gender) == fo::GENDER_FEMALE
					? armorPro->item.armor.femaleFID
					: armorPro->item.armor.maleFID;

				if (armorFid != -1) {
					indexNum = armorFid;
				}
			}
		}
		auto itemInHand = fo::func::intface_is_item_right_hand()
			? fo::var::i_rhand
			: fo::var::i_lhand;

		if (itemInHand != nullptr) {
			auto itemPro = fo::GetProto(itemInHand->protoId);
			if (itemPro->item.type == fo::item_type_weapon) {
				weaponAnimCode = itemPro->item.weapon.animationCode;
			}
		}
		fid = fo::func::art_id(fo::OBJ_TYPE_CRITTER, indexNum, 0, weaponAnimCode, 0);
	} else {
		fid = fo::var::inven_dude->artFid;
	}
	fo::var::i_fid = fid;
	sf::Inventory::InvokeAdjustFid(fid);
	return fo::var::i_fid;
}

static void __declspec(naked) adjust_fid_hack() {
	__asm {
		push ecx;
		push edx;
		call Inventory::adjust_fid; // return fid
		pop  edx;
		pop  ecx;
		retn;
	}
}

void Inventory::init() {
	// Replace functions
	sf::MakeJump(fo::funcoffs::adjust_fid_, adjust_fid_hack); // 0x4716E8
}

}
