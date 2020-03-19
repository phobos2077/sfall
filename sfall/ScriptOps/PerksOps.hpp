/*
 *    sfall
 *    Copyright (C) 2008, 2009, 2010, 2012  The sfall team
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "main.h"
#include "Perks.h"
#include "ScriptExtender.h"

static void __declspec(naked) GetPerkOwed() {
	__asm {
		movzx edx, byte ptr ds:[_free_perk];
		_J_RET_VAL_TYPE(VAR_TYPE_INT);
//		retn;
	}
}

static void __declspec(naked) SetPerkOwed() {
	__asm {
		_GET_ARG_INT(end);
		and  eax, 0xFF;
		cmp  eax, 250;
		jg   end;
		mov  byte ptr ds:[_free_perk], al;
end:
		retn;
	}
}

static void __declspec(naked) set_perk_freq() {
	__asm {
		mov  esi, ecx;
		_GET_ARG_INT(end);
		push eax;
		call SetPerkFreq;
end:
		mov  ecx, esi;
		retn;
	}
}

static void __declspec(naked) GetPerkAvailable() {
	__asm {
		pushad;
		mov ecx, eax;
		call interpretPopShort_;
		mov edx, eax;
		mov eax, ecx;
		call interpretPopLong_;
		cmp dx, VAR_TYPE_INT;
		jnz fail;
		cmp eax, PERK_count;
		jge fail;
		mov edx, eax;
		mov eax, ds:[_obj_dude];
		call perk_can_add_;
		mov edx, eax;
		jmp end;
fail:
		xor edx, edx;
end:
		mov eax, ecx
		call interpretPushLong_;
		mov edx, VAR_TYPE_INT;
		mov eax, ecx;
		call interpretPushShort_;
		popad;
		retn;
	}
}

static void __declspec(naked) funcSetPerkName() {
	__asm {
		pushad;
		mov ecx, eax;
		call interpretPopShort_;
		mov esi, eax;
		mov eax, ecx;
		call interpretPopLong_;
		mov edi, eax;
		mov eax, ecx;
		call interpretPopShort_;
		mov edx, eax;
		mov eax, ecx;
		call interpretPopLong_;
		cmp dx, VAR_TYPE_INT;
		jnz end;
		cmp si, VAR_TYPE_STR2;
		jz next;
		cmp si, VAR_TYPE_STR;
		jnz end;
next:
		mov ebx, edi;
		mov edx, esi;
		mov esi, eax;
		mov eax, ecx;
		call interpretGetString_;
		push eax;
		push esi;
		call SetPerkName;
		jmp end;
end:
		popad;
		retn;
	}
}

static void __declspec(naked) funcSetPerkDesc() {
	__asm {
		pushad;
		mov ecx, eax;
		call interpretPopShort_;
		mov esi, eax;
		mov eax, ecx;
		call interpretPopLong_;
		mov edi, eax;
		mov eax, ecx;
		call interpretPopShort_;
		mov edx, eax;
		mov eax, ecx;
		call interpretPopLong_;
		cmp dx, VAR_TYPE_INT;
		jnz end;
		cmp si, VAR_TYPE_STR2;
		jz next;
		cmp si, VAR_TYPE_STR;
		jnz end;
next:
		mov ebx, edi;
		mov edx, esi;
		mov esi, eax;
		mov eax, ecx;
		call interpretGetString_;
		push eax;
		push esi;
		call SetPerkDesc;
		jmp end;
end:
		popad;
		retn;
	}
}

static void __declspec(naked) funcSetPerkValue() {
	__asm { // edx - opcode
		push edi;
		push ecx;
		sub  edx, 0x5E0 - 8; // offset of value into perk struct: edx = ((edx/4) - 0x178 + 0x8) * 4
		mov  edi, edx;
		_GET_ARG(ecx, esi);
		mov  eax, ebx;
		_GET_ARG_INT(end);
		cmp  si, VAR_TYPE_INT;
		jne  end;
		push ecx;      // value
		mov  edx, edi; // param (offset)
		mov  ecx, eax; // perk id
		call SetPerkValue;
end:
		pop  ecx;
		pop  edi;
		retn;
	}
}

static void __declspec(naked) fSetSelectablePerk() {
	__asm {
		pushad;
		mov ecx, eax;
		call interpretPopShort_;
		push eax;
		mov eax, ecx;
		call interpretPopLong_;
		push eax;
		mov eax, ecx;
		call interpretPopShort_;
		push eax;
		mov eax, ecx;
		call interpretPopLong_;
		push eax;
		mov eax, ecx;
		call interpretPopShort_;
		push eax;
		mov eax, ecx;
		call interpretPopLong_;
		push eax;
		mov eax, ecx;
		call interpretPopShort_;
		push eax;
		mov eax, ecx;
		call interpretPopLong_;
		push eax;

		movzx eax, word ptr [esp + 12];
		cmp eax, VAR_TYPE_INT;
		jne fail;
		movzx eax, word ptr [esp + 20];
		cmp eax, VAR_TYPE_INT;
		jne fail;
		movzx eax, word ptr ds:[esp + 4];
		cmp eax, VAR_TYPE_STR2;
		je next1;
		cmp eax, VAR_TYPE_STR;
		jne fail;
next1:
		movzx eax, word ptr ds:[esp + 28];
		cmp eax, VAR_TYPE_STR2;
		je next2;
		cmp eax, VAR_TYPE_STR;
		jne fail;
next2:
		mov eax, ecx;
		mov edx, [esp + 28];
		mov ebx, [esp + 24];
		call interpretGetString_;
		push eax;
		mov eax, [esp + 20];
		push eax;
		mov eax, [esp + 16];
		push eax;
		mov eax, ecx;
		mov edx, [esp + 16];
		mov ebx, [esp + 12];
		call interpretGetString_;
		push eax;

		call SetSelectablePerk;
fail:
		add esp, 32;
		popad;
		retn;
	}
}

static void __declspec(naked) fSetFakePerk() {
	__asm {
		pushad;
		mov ecx, eax;
		call interpretPopShort_;
		push eax;
		mov eax, ecx;
		call interpretPopLong_;
		push eax;
		mov eax, ecx;
		call interpretPopShort_;
		push eax;
		mov eax, ecx;
		call interpretPopLong_;
		push eax;
		mov eax, ecx;
		call interpretPopShort_;
		push eax;
		mov eax, ecx;
		call interpretPopLong_;
		push eax;
		mov eax, ecx;
		call interpretPopShort_;
		push eax;
		mov eax, ecx;
		call interpretPopLong_;
		push eax;

		movzx eax, word ptr [esp + 12];
		cmp eax, VAR_TYPE_INT;
		jne fail;
		movzx eax, word ptr [esp + 20];
		cmp eax, VAR_TYPE_INT;
		jne fail;
		movzx eax, word ptr ds:[esp + 4];
		cmp eax, VAR_TYPE_STR2;
		je next1;
		cmp eax, VAR_TYPE_STR;
		jne fail;
next1:
		movzx eax, word ptr ds:[esp + 28];
		cmp eax, VAR_TYPE_STR2;
		je next2;
		cmp eax, VAR_TYPE_STR;
		jne fail;
next2:
		mov eax, ecx;
		mov edx, [esp + 28];
		mov ebx, [esp + 24];
		call interpretGetString_;
		push eax;
		mov eax, [esp + 20];
		push eax;
		mov eax, [esp + 16];
		push eax;
		mov eax, ecx;
		mov edx, [esp + 16];
		mov ebx, [esp + 12];
		call interpretGetString_;
		push eax;

		call SetFakePerk;
fail:
		add esp, 32;
		popad;
		retn;
	}
}

static void __declspec(naked) fSetFakeTrait() {
	__asm {
		push ebx;
		push ecx;
		push edx;
		push edi;
		push esi;
		mov ecx, eax;
		call interpretPopShort_;
		push eax;
		mov eax, ecx;
		call interpretPopLong_;
		push eax;
		mov eax, ecx;
		call interpretPopShort_;
		push eax;
		mov eax, ecx;
		call interpretPopLong_;
		push eax;
		mov eax, ecx;
		call interpretPopShort_;
		push eax;
		mov eax, ecx;
		call interpretPopLong_;
		push eax;
		mov eax, ecx;
		call interpretPopShort_;
		push eax;
		mov eax, ecx;
		call interpretPopLong_;
		push eax;

		movzx eax, word ptr [esp + 12];
		cmp eax, VAR_TYPE_INT;
		jne fail;
		movzx eax, word ptr [esp + 20];
		cmp eax, VAR_TYPE_INT;
		jne fail;
		movzx eax, word ptr ds:[esp + 4];
		cmp eax, VAR_TYPE_STR2;
		je next1;
		cmp eax, VAR_TYPE_STR;
		jne fail;
next1:
		movzx eax, word ptr ds:[esp + 28];
		cmp eax, VAR_TYPE_STR2;
		je next2;
		cmp eax, VAR_TYPE_STR;
		jne fail;
next2:
		mov eax, ecx;
		mov edx, [esp + 28];
		mov ebx, [esp + 24];
		call interpretGetString_;
		push eax;
		mov eax, [esp + 20];
		push eax;
		mov eax, [esp + 16];
		push eax;
		mov eax, ecx;
		mov edx, [esp + 16];
		mov ebx, [esp + 12];
		call interpretGetString_;
		push eax;

		call SetFakeTrait;
fail:
		add esp, 32;
		pop esi;
		pop edi;
		pop edx;
		pop ecx;
		pop ebx;
		retn;
	}
}

static void __declspec(naked) fSetPerkboxTitle() {
	__asm {
		mov  esi, ecx;
		push ebx;
		mov  ecx, eax; // keep script
		_GET_ARG(ebx, edx);
		cmp  dx, VAR_TYPE_STR2;
		jz   next;
		cmp  dx, VAR_TYPE_STR;
		jnz  end;
next:
		mov  eax, ecx; // script
		call interpretGetString_;
		mov  ecx, eax;
		call SetPerkboxTitle;
end:
		mov  ecx, esi;
		pop  ebx;
		retn;
	}
}

static void __declspec(naked) fIgnoreDefaultPerks() {
	__asm {
		mov  esi, ecx;
		call IgnoreDefaultPerks;
		mov  ecx, esi;
		retn;
	}
}

static void __declspec(naked) fRestoreDefaultPerks() {
	__asm {
		mov  esi, ecx;
		call RestoreDefaultPerks;
		mov  ecx, esi;
		retn;
	}
}

static void __declspec(naked) fClearSelectablePerks() {
	__asm {
		mov  esi, ecx;
		call ClearSelectablePerks;
		mov  ecx, esi;
		retn;
	}
}

static void __declspec(naked) fHasFakePerk() {
	__asm {
		push ebx;
		push ecx;
		push edx;
		push edi;
		mov edi, eax;
		call interpretPopShort_;
		mov edx, eax;
		mov eax, edi;
		call interpretPopLong_;
		cmp dx, VAR_TYPE_STR2;
		jz next;
		cmp dx, VAR_TYPE_STR;
		jnz end;
next:
		mov ebx, eax;
		mov eax, edi;
		call interpretGetString_;
		push eax;
		call HasFakePerk;
end:
		mov edx, eax;
		mov eax, edi;
		call interpretPushLong_;
		mov eax, edi;
		mov edx, VAR_TYPE_INT;
		call interpretPushShort_;
		pop edi;
		pop edx;
		pop ecx;
		pop ebx;
		retn;
	}
}
static void __declspec(naked) fHasFakeTrait() {
	__asm {
		push ebx;
		push ecx;
		push edx;
		push edi;
		mov edi, eax;
		call interpretPopShort_;
		mov edx, eax;
		mov eax, edi;
		call interpretPopLong_;
		cmp dx, VAR_TYPE_STR2;
		jz next;
		cmp dx, VAR_TYPE_STR;
		jnz end;
next:
		mov ebx, eax;
		mov eax, edi;
		call interpretGetString_;
		push eax;
		call HasFakeTrait;
end:
		mov edx, eax;
		mov eax, edi;
		call interpretPushLong_;
		mov eax, edi;
		mov edx, VAR_TYPE_INT;
		call interpretPushShort_;
		pop edi;
		pop edx;
		pop ecx;
		pop ebx;
		retn;
	}
}

static void __declspec(naked) fAddPerkMode() {
	__asm {
		mov  esi, ecx;
		_GET_ARG_INT(end);
		push eax;
		call AddPerkMode;
end:
		mov  ecx, esi;
		retn;
	}
}

static void __declspec(naked) remove_trait() {
	__asm {
		_GET_ARG_INT(end);
		test eax, eax;
		jl   end;
		mov  edx, -1;
		cmp  eax, ds:[_pc_trait];
		jne  next;
		mov  esi, ds:[_pc_trait2];
		mov  ds:[_pc_trait], esi;
		mov  ds:[_pc_trait2], edx;
		retn;
next:
		cmp  eax, ds:[_pc_trait2];
		jne  end;
		mov  ds:[_pc_trait2], edx;
end:
		retn;
	}
}

static void __declspec(naked) SetPyromaniacMod() {
	__asm {
		mov  esi, ecx;
		_GET_ARG_INT(end);
		push eax;
		push 0x424AB6;
		call SafeWrite8;
end:
		mov  ecx, esi;
		retn;
	}
}

static void __declspec(naked) fApplyHeaveHoFix() {
	__asm {
		mov  esi, ecx;
		call ApplyHeaveHoFix;
		mov  ecx, esi;
		retn;
	}
}

static void __declspec(naked) SetSwiftLearnerMod() {
	__asm {
		mov  esi, ecx;
		_GET_ARG_INT(end);
		push eax;
		push 0x4AFAE2;
		call SafeWrite32;
end:
		mov  ecx, esi;
		retn;
	}
}

static void __declspec(naked) perk_can_add_hook() {
	__asm {
		call stat_pc_get_;
		add  eax, PerkLevelMod;
		js   jneg; // level < 0
		retn;
jneg:
		xor  eax, eax;
		retn;
	}
}

static void __fastcall SetPerkLevelMod2(long mod) {
	static bool perkLevelModPatch = false;
	if (mod < -25 || mod > 25) return;
	PerkLevelMod = mod;

	if (perkLevelModPatch) return;
	perkLevelModPatch = true;
	HookCall(0x49687F, perk_can_add_hook);
}

static void __declspec(naked) SetPerkLevelMod() {
	__asm {
		mov  esi, ecx;
		_GET_ARG_INT(end);
		mov  ecx, eax;
		call SetPerkLevelMod2;
end:
		mov  ecx, esi;
		retn;
	}
}

static void sf_add_trait() {
	if (*(DWORD*)(*(DWORD*)_obj_dude + 0x64) != PID_Player) {
		opHandler.printOpcodeError("add_trait() - traits can be added only to the player.");
		opHandler.setReturn(-1);
		return;
	}
	long traitId = opHandler.arg(0).rawValue();
	if (traitId >= TRAIT_fast_metabolism && traitId <= TRAIT_gifted) {
		if (ptr_pc_traits[0] == -1) {
			ptr_pc_traits[0] = traitId;
		} else if (ptr_pc_traits[0] != traitId && ptr_pc_traits[1] == -1) {
			ptr_pc_traits[1] = traitId;
		} else {
			opHandler.printOpcodeError("add_trait() - cannot add the trait ID: %d", traitId);
		}
	} else {
		opHandler.printOpcodeError("add_trait() - invalid trait ID.");
	}
}
