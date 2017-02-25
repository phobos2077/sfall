/*
*    sfall
*    Copyright (C) 2008-2016  The sfall team
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

#include <math.h>
#include <stdio.h>

#include "..\main.h"
#include "..\FalloutEngine\Fallout2.h"
#include "Graphics.h"
#include "ScriptExtender.h"
#include "Timer.h"

#include "Misc.h"

// TODO: split this into smaller files

static char mapName[65];
static char configName[65];
static char patchName[65];
static char versionString[65];

static const char* debugLog = "LOG";
static const char* debugGnw = "GNW";

static int* scriptDialog = nullptr;

//GetTickCount calls
static const DWORD offsetsA[] = {
	0x4C8D34, 0x4C9375, 0x4C9384, 0x4C93C0, 0x4C93E8, 0x4C9D2E, 0x4FE01E,
};

//Delayed GetTickCount calls
static const DWORD offsetsB[] = {
	0x4FDF64,
};

//timeGetTime calls
static const DWORD offsetsC[] = {
	0x4A3179, 0x4A325D, 0x4F482B, 0x4F4E53, 0x4F5542, 0x4F56CC, 0x4F59C6,
	0x4FE036,
};

static const DWORD PutAwayWeapon[] = {
	0x411EA2, // action_climb_ladder_
	0x412046, // action_use_an_item_on_object_
	0x41224A, // action_get_an_object_
	0x4606A5, // intface_change_fid_animate_
	0x472996, // invenWieldFunc_
};

static const DWORD origFuncPosA = 0x6C0200;
static const DWORD origFuncPosB = 0x6C03B0;
static const DWORD origFuncPosC = 0x6C0164;

static const DWORD getLocalTimePos = 0x4FDF58;

static const DWORD dinputPos = 0x50FB70;
static const DWORD DDrawPos = 0x50FB5C;

static DWORD AddrGetTickCount;
static DWORD AddrGetLocalTime;

static DWORD ViewportX;
static DWORD ViewportY;

static DWORD KarmaFrmCount;
static DWORD* KarmaFrms;
static int* KarmaPoints;

static const DWORD FastShotFixF1[] = {
	0x478BB8, 0x478BC7, 0x478BD6, 0x478BEA, 0x478BF9, 0x478C08, 0x478C2F,
};

static const DWORD script_dialog_msgs[] = {
	0x4A50C2, 0x4A5169, 0x4A52FA, 0x4A5302, 0x4A6B86, 0x4A6BE0, 0x4A6C37,
};


static double FadeMulti;
static __declspec(naked) void FadeHook() {
	__asm {
		pushf;
		push ebx;
		fild[esp];
		fmul FadeMulti;
		fistp[esp];
		pop ebx;
		popf;
		call FuncOffs::fadeSystemPalette_;
		retn;
	}
}

static __declspec(naked) void GetDateWrapper() {
	__asm {
		push ecx;
		push esi;
		push ebx;
		call FuncOffs::game_time_date_;
		mov ecx, ds:[VARPTR_pc_proto + 0x4C];
		pop esi;
		test esi, esi;
		jz end;
		add ecx, [esi];
		mov[esi], ecx;
end:
		pop esi;
		pop ecx;
		retn;
	}
}

static void TimerReset() {
	VarPtr::fallout_game_time = 0;
	// used as additional years indicator
	VarPtr::pc_proto.base_stat_unarmed_damage += 13;
}

static double TickFrac = 0;
static double MapMulti = 1;
static double MapMulti2 = 1;
void _stdcall SetMapMulti(float d) {
	MapMulti2 = d;
}

static __declspec(naked) void PathfinderFix3() {
	__asm {
		xor eax, eax;
		retn;
	}
}

static DWORD _stdcall PathfinderFix2(DWORD perkLevel, DWORD ticks) {
	double d = MapMulti*MapMulti2;
	if (perkLevel == 1) d *= 0.75;
	else if (perkLevel == 2) d *= 0.5;
	else if (perkLevel == 3) d *= 0.25;
	d = ((double)ticks)*d + TickFrac;
	TickFrac = modf(d, &d);
	return (DWORD)d;
}

static __declspec(naked) void PathfinderFix() {
	__asm {
		push eax;
		mov eax, ds:[VARPTR_obj_dude];
		mov edx, PERK_pathfinder;
		call FuncOffs::perk_level_;
		push eax;
		call PathfinderFix2;
		call FuncOffs::inc_game_time_;
		retn;
	}
}

static int mapSlotsScrollMax = 27 * (17 - 7);

static __declspec(naked) void ScrollCityListHook() {
	__asm {
		push ebx;
		mov ebx, ds:[0x672F10];
		test eax, eax;
		jl up;
		cmp ebx, mapSlotsScrollMax;
		je end;
		jmp run;
up:
		test ebx, ebx;
		jz end;
run:
		call FuncOffs::wmInterfaceScrollTabsStart_;
end:
		pop ebx;
		retn;
	}
}

static DWORD wp_delay;
static void __declspec(naked) worldmap_patch() {
	__asm {
		pushad;
		call RunGlobalScripts3;
		mov ecx, wp_delay;
tck:
		mov eax, ds : [0x50fb08];
		call FuncOffs::elapsed_time_;
		cmp eax, ecx;
		jl tck;
		call FuncOffs::get_time_;
		mov ds : [0x50fb08], eax;
		popad;
		jmp FuncOffs::get_input_;
	}
}

static void __declspec(naked) WorldMapEncPatch1() {
	__asm {
		inc dword ptr ds : [VARPTR_wmLastRndTime]
		call FuncOffs::wmPartyWalkingStep_;
		retn;
	}
}

static void __declspec(naked) WorldMapEncPatch2() {
	__asm {
		mov dword ptr ds : [VARPTR_wmLastRndTime], 0;
		retn;
	}
}

static void __declspec(naked) WorldMapEncPatch3() {
	__asm {
		mov eax, ds:[VARPTR_wmLastRndTime];
		retn;
	}
}

static DWORD WorldMapEncounterRate;
static void __declspec(naked) wmWorldMapFunc_hook() {
	__asm {
		inc  dword ptr ds:[VARPTR_wmLastRndTime];
		jmp  FuncOffs::wmPartyWalkingStep_;
	}
}

static void __declspec(naked) wmRndEncounterOccurred_hack() {
	__asm {
		xor  ecx, ecx;
		cmp  edx, WorldMapEncounterRate;
		retn;
	}
}

static void __declspec(naked) Combat_p_procFix() {
	__asm {
		push eax;

		mov eax, dword ptr ds : [VARPTR_combat_state];
		cmp eax, 3;
		jnz end_cppf;

		push esi;
		push ebx;
		push edx;

		mov esi, VARPTR_main_ctd;
		mov eax, [esi];
		mov ebx, [esi + 0x20];
		xor edx, edx;
		mov eax, [eax + 0x78];
		call FuncOffs::scr_set_objs_;
		mov eax, [esi];

		cmp dword ptr ds : [esi + 0x2c], +0x0;
		jng jmp1;

		test byte ptr ds : [esi + 0x15], 0x1;
		jz jmp1;
		mov edx, 0x2;
		jmp jmp2;
jmp1:
		mov edx, 0x1;
jmp2:
		mov eax, [eax + 0x78];
		call FuncOffs::scr_set_ext_param_;
		mov eax, [esi];
		mov edx, 0xd;
		mov eax, [eax + 0x78];
		call FuncOffs::exec_script_proc_;
		pop edx;
		pop ebx;
		pop esi;

end_cppf:
		pop eax;
		call FuncOffs::stat_level_;

		retn;
	}
}

static void __declspec(naked) WeaponAnimHook() {
	__asm {
		cmp edx, 11;
		je c11;
		cmp edx, 15;
		je c15;
		jmp FuncOffs::art_get_code_;
c11:
		mov edx, 16;
		jmp FuncOffs::art_get_code_;
c15:
		mov edx, 17;
		jmp FuncOffs::art_get_code_;
	}
}

static double wm_nexttick;
static double wm_wait;
static bool wm_usingperf;
static __int64 wm_perfadd;
static __int64 wm_perfnext;
static DWORD WorldMapLoopCount;
static void WorldMapSpeedPatch3() {
	RunGlobalScripts3();
	if (wm_usingperf) {
		__int64 timer;
		while (true) {
			QueryPerformanceCounter((LARGE_INTEGER*)&timer);
			if (timer > wm_perfnext) break;
			Sleep(0);
		}
		if (wm_perfnext + wm_perfadd < timer) wm_perfnext = timer + wm_perfadd;
		else wm_perfnext += wm_perfadd;
	} else {
		DWORD tick;
		DWORD nexttick = (DWORD)wm_nexttick;
		while ((tick = GetTickCount()) < nexttick) Sleep(0);
		if (nexttick + wm_wait < tick) wm_nexttick = tick + wm_wait;
		else wm_nexttick += wm_wait;
	}
}

static void __declspec(naked) WorldMapSpeedPatch2() {
	__asm {
		pushad;
		call WorldMapSpeedPatch3;
		popad;
		call FuncOffs::get_input_;
		retn;
	}
}

static void __declspec(naked) WorldMapSpeedPatch() {
	__asm {
		pushad;
		call RunGlobalScripts3;
		popad;
		push ecx;
		mov ecx, WorldMapLoopCount;
ls:
		mov eax, eax;
		loop ls;
		pop ecx;
		call FuncOffs::get_input_;
		retn;
	}
}

//Only used if the world map speed patch is disabled, so that world map scripts are still run
static void WorldMapHook() {
	__asm {
		pushad;
		call RunGlobalScripts3;
		popad;
		call FuncOffs::get_input_;
		retn;
	}
}

static void __declspec(naked) ViewportHook() {
	__asm {
		call FuncOffs::wmWorldMapLoadTempData_;
		mov eax, ViewportX;
		mov ds : [VARPTR_wmWorldOffsetX], eax
		mov eax, ViewportY;
		mov ds : [VARPTR_wmWorldOffsetY], eax;
		retn;
	}
}

static void __declspec(naked) intface_rotate_numbers_hack() {
	__asm {
		push edi
		push ebp
		sub  esp, 0x54
		// ebx=old value, ecx=new value
		cmp  ebx, ecx
		je   end
		mov  ebx, ecx
		jg   decrease
		dec  ebx
		jmp  end
decrease:
		test ecx, ecx
		jl   negative
		inc  ebx
		jmp  end
negative:
		xor  ebx, ebx
end:
		push 0x460BA6
		retn
	}
}

static void __declspec(naked) register_object_take_out_hack() {
	__asm {
		push ecx
		push eax
		mov  ecx, edx                             // ID1
		mov  edx, [eax + 0x1C]                      // cur_rot
		inc  edx
		push edx                                  // ID3
		xor  ebx, ebx                             // ID2
		mov  edx, [eax + 0x20]                      // fid
		and edx, 0xFFF                           // Index
		xor eax, eax
		inc  eax                                  // Obj_Type
		call FuncOffs::art_id_
		xor  ebx, ebx
		dec  ebx
		xchg edx, eax
		pop  eax
		call FuncOffs::register_object_change_fid_
		pop  ecx
		xor  eax, eax
		retn
	}
}

static void __declspec(naked) gdAddOptionStr_hack() {
	__asm {
		mov  ecx, ds:[VARPTR_gdNumOptions]
		add  ecx, '1'
		push ecx
		push 0x4458FA
		retn
	}
}

static DWORD _stdcall DrawCardHook2() {
	int reputation = VarPtr::game_global_vars[GVAR_PLAYER_REPUTATION];
	for (DWORD i = 0; i < KarmaFrmCount - 1; i++) {
		if (reputation < KarmaPoints[i]) return KarmaFrms[i];
	}
	return KarmaFrms[KarmaFrmCount - 1];
}

static void __declspec(naked) DrawCardHook() {
	__asm {
		cmp ds : [VARPTR_info_line], 10;
		jne skip;
		cmp eax, 0x30;
		jne skip;
		push ecx;
		push edx;
		call DrawCardHook2;
		pop edx;
		pop ecx;
skip:
		jmp FuncOffs::DrawCard_;
	}
}

static void __declspec(naked) ScienceCritterCheckHook() {
	__asm {
		cmp esi, ds:[VARPTR_obj_dude];
		jne end;
		mov eax, 10;
		retn;
end:
		jmp FuncOffs::critter_kill_count_type_;
	}
}

static const DWORD FastShotTraitFixEnd1 = 0x478E7F;
static const DWORD FastShotTraitFixEnd2 = 0x478E7B;
static void __declspec(naked) FastShotTraitFix() {
	__asm {
		test eax, eax;				// does player have Fast Shot trait?
		je ajmp;				// skip ahead if no
		mov edx, ecx;				// argument for item_w_range_: hit_mode
		mov eax, ebx;				// argument for item_w_range_: pointer to source_obj (always dude_obj due to code path)
		call FuncOffs::item_w_range_;			// get weapon's range
		cmp eax, 0x2;				// is weapon range less than or equal 2 (i.e. melee/unarmed attack)?
		jle ajmp;				// skip ahead if yes
		xor eax, eax;				// otherwise, disallow called shot attempt
		jmp bjmp;
ajmp:
		jmp FastShotTraitFixEnd1;		// continue processing called shot attempt
bjmp:
		jmp FastShotTraitFixEnd2;		// clean up and exit function item_w_called_shot
	}
}

static void __declspec(naked) removeDatabase() {
	__asm {
		cmp  eax, -1
		je   end
		mov  ebx, ds:[VARPTR_paths]
		mov  ecx, ebx
nextPath:
		mov  edx, [esp+0x104+4+4]                 // path_patches
		mov  eax, [ebx]                           // database.path
		call FuncOffs::stricmp_
		test eax, eax                             // found path?
		jz   skip                                 // Yes
		mov  ecx, ebx
		mov  ebx, [ebx+0xC]                       // database.next
		jmp  nextPath
skip:
		mov  eax, [ebx+0xC]                       // database.next
		mov  [ecx+0xC], eax                       // database.next
		xchg ebx, eax
		cmp  eax, ecx
		jne  end
		mov  ds:[VARPTR_paths], ebx
end:
		retn
	}
}

static void __declspec(naked) game_init_databases_hack1() {
	__asm {
		call removeDatabase
		mov  ds:[VARPTR_master_db_handle], eax
		retn
	}
}

static void __declspec(naked) game_init_databases_hack2() {
	__asm {
		cmp  eax, -1
		je   end
		mov  eax, ds:[VARPTR_master_db_handle]
		mov  eax, [eax]                           // eax = master_patches.path
		call FuncOffs::xremovepath_
		dec  eax                                  // remove path (critter_patches == master_patches)?
		jz   end                                  // Yes
		inc  eax
		call removeDatabase
end:
		mov  ds:[VARPTR_critter_db_handle], eax
		retn
	}
}

static void __declspec(naked) game_init_databases_hook() {
// eax = _master_db_handle
	__asm {
		mov  ecx, ds:[VARPTR_critter_db_handle]
		mov  edx, ds:[VARPTR_paths]
		jecxz skip
		mov  [ecx+0xC], edx                       // critter_patches.next->_paths
		mov  edx, ecx
skip:
		mov  [eax+0xC], edx                       // master_patches.next
		mov  ds:[VARPTR_paths], eax
		retn
	}
}

static void __declspec(naked) ReloadHook() {
	__asm {
		push eax;
		push ebx;
		push edx;
		mov eax, dword ptr ds:[VARPTR_obj_dude];
		call FuncOffs::register_clear_;
		xor eax, eax;
		inc eax;
		call FuncOffs::register_begin_;
		xor edx, edx;
		xor ebx, ebx;
		mov eax, dword ptr ds:[VARPTR_obj_dude];
		dec ebx;
		call FuncOffs::register_object_animate_;
		call FuncOffs::register_end_;
		pop edx;
		pop ebx;
		pop eax;
		jmp FuncOffs::gsound_play_sfx_file_;
	}
}


static const DWORD CorpseHitFix2_continue_loop1 = 0x48B99B;
static void __declspec(naked) CorpseHitFix2() {
	__asm {
		push eax;
		mov eax, [eax];
		call FuncOffs::critter_is_dead_; // found some object, check if it's a dead critter
		test eax, eax;
		pop eax;
		jz really_end; // if not, allow breaking the loop (will return this object)
		jmp CorpseHitFix2_continue_loop1; // otherwise continue searching

really_end:
		mov     eax, [eax];
		pop     ebp;
		pop     edi;
		pop     esi;
		pop     ecx;
		retn;
	}
}

static const DWORD CorpseHitFix2_continue_loop2 = 0x48BA0B;
// same logic as above, for different loop
static void __declspec(naked) CorpseHitFix2b() {
	__asm {
		mov eax, [edx];
		call FuncOffs::critter_is_dead_;
		test eax, eax;
		jz really_end;
		jmp CorpseHitFix2_continue_loop2;

really_end:
		mov     eax, [edx];
		pop     ebp;
		pop     edi;
		pop     esi;
		pop     ecx;
		retn;
	}
}

static DWORD RetryCombatLastAP;
static DWORD RetryCombatMinAP;
static void __declspec(naked) RetryCombatHook() {
	__asm {
		mov RetryCombatLastAP, 0;
retry:
		mov eax, esi;
		xor edx, edx;
		call FuncOffs::combat_ai_;
process:
		cmp dword ptr ds:[VARPTR_combat_turn_running], 0;
		jle next;
		call FuncOffs::process_bk_;
		jmp process;
next:
		mov eax, [esi+0x40];
		cmp eax, RetryCombatMinAP;
		jl end;
		cmp eax, RetryCombatLastAP;
		je end;
		mov RetryCombatLastAP, eax;
		jmp retry;
end:
		retn;
	}
}

static const DWORD NPCStage6Fix1End = 0x493D16;
static const DWORD NPCStage6Fix2End = 0x49423A;
static void __declspec(naked) NPCStage6Fix1() {
	__asm {
		mov eax,0xcc;				// set record size to 204 bytes
		imul eax,edx;				// multiply by number of NPC records in party.txt
		call FuncOffs::mem_malloc_;			// malloc the necessary memory
		mov edx,dword ptr ds:[VARPTR_partyMemberMaxCount];	// retrieve number of NPC records in party.txt
		mov ebx,0xcc;				// set record size to 204 bytes
		imul ebx,edx;				// multiply by number of NPC records in party.txt
		jmp NPCStage6Fix1End;			// call memset to set all malloc'ed memory to 0
	}
}

static void __declspec(naked) NPCStage6Fix2() {
	__asm {
		mov eax,0xcc;				// record size is 204 bytes
		imul edx,eax;				// multiply by NPC number as listed in party.txt
		mov eax,dword ptr ds:[VARPTR_partyMemberAIOptions];	// get starting offset of internal NPC table
		jmp NPCStage6Fix2End;			// eax+edx = offset of specific NPC record
	}
}

static const DWORD ScannerHookRet=0x41BC1D;
static const DWORD ScannerHookFail=0x41BC65;
static void __declspec(naked) ScannerAutomapHook() {
	__asm {
		mov eax, ds:[VARPTR_obj_dude];
		mov edx, PID_MOTION_SENSOR;
		call FuncOffs::inven_pid_is_carried_ptr_;
		test eax, eax;
		jz fail;
		mov edx, eax;
		jmp ScannerHookRet;
fail:
		jmp ScannerHookFail;
	}
}

static void __declspec(naked) objCanSeeObj_ShootThru_Fix() {//(EAX *objStruct, EDX hexNum1, EBX hexNum2, ECX ?, stack1 **ret_objStruct, stack2 flags)
	__asm {
		push esi
		push edi

		push FuncOffs::obj_shoot_blocking_at_ //arg3 check hex objects func pointer
		mov esi, 0x20//arg2 flags, 0x20 = check shootthru
		push esi
		mov edi, dword ptr ss : [esp + 0x14] //arg1 **ret_objStruct
		push edi
		call FuncOffs::make_straight_path_func_;//(EAX *objStruct, EDX hexNum1, EBX hexNum2, ECX ?, stack1 **ret_objStruct, stack2 flags, stack3 *check_hex_objs_func)

		pop edi
		pop esi
		ret 0x8
	}
}

static void __declspec(naked) wmTownMapFunc_hack() {
	__asm {
		cmp  edx, 0x31
		jl   end
		cmp  edx, ecx
		jge  end
		push edx
		sub  edx, 0x31
		lea  eax, ds:0[edx*8]
		sub  eax, edx
		pop  edx
		cmp  dword ptr [edi+eax*4+0x0], 0         // Visited
		je   end
		cmp  dword ptr [edi+eax*4+0x4], -1        // Xpos
		je   end
		cmp  dword ptr [edi+eax*4+0x8], -1        // Ypos
		je   end
		retn
end:
		pop  eax                                  // destroy the return address
		push 0x4C4976
		retn
	}
}

static char KarmaGainMsg[128];
static char KarmaLossMsg[128];
static void _stdcall SetKarma(int value) {
	int old = VarPtr::game_global_vars[GVAR_PLAYER_REPUTATION];
	old = value - old;
	char buf[64];
	if (old == 0) return;
	if (old > 0) {
		sprintf_s(buf, KarmaGainMsg, old);
	} else {
		sprintf_s(buf, KarmaLossMsg, -old);
	}
	Wrapper::display_print(buf);
}

static void __declspec(naked) SetGlobalVarWrapper() {
	__asm {
		test eax, eax;
		jnz end;
		pushad;
		push edx;
		call SetKarma;
		popad;
end:
		jmp FuncOffs::game_set_global_var_;
	}
}

static const DWORD EncounterTableSize[] = {
	0x4BD1A3, 0x4BD1D9, 0x4BD270, 0x4BD604, 0x4BDA14, 0x4BDA44, 0x4BE707,
	0x4C0815, 0x4C0D4A, 0x4C0FD4,
};

static char StartMaleModelName[65];
char DefaultMaleModelName[65];
static char StartFemaleModelName[65];
char DefaultFemaleModelName[65];

void ApplySpeedPatch() {
	if (GetPrivateProfileIntA("Speed", "Enable", 0, ini)) {
		dlog("Applying speed patch.", DL_INIT);
		AddrGetTickCount = (DWORD)&FakeGetTickCount;
		AddrGetLocalTime = (DWORD)&FakeGetLocalTime;

		for (int i = 0; i < sizeof(offsetsA) / 4; i++) {
			SafeWrite32(offsetsA[i], (DWORD)&AddrGetTickCount);
		}
		dlog(".", DL_INIT);
		for (int i = 0; i < sizeof(offsetsB) / 4; i++) {
			SafeWrite32(offsetsB[i], (DWORD)&AddrGetTickCount);
		}
		dlog(".", DL_INIT);
		for (int i = 0; i < sizeof(offsetsC) / 4; i++) {
			SafeWrite32(offsetsC[i], (DWORD)&AddrGetTickCount);
		}
		dlog(".", DL_INIT);

		SafeWrite32(getLocalTimePos, (DWORD)&AddrGetLocalTime);
		TimerInit();
		dlogr(" Done", DL_INIT);
	}
}

void ApplyInputPatch() {
	//if(GetPrivateProfileIntA("Input", "Enable", 0, ini)) {
		dlog("Applying input patch.", DL_INIT);
		SafeWriteStr(dinputPos, "ddraw.dll");
		AvailableGlobalScriptTypes |= 1;
		dlogr(" Done", DL_INIT);
	//}
}

void ApplyGraphicsPatch() {
	DWORD fadeMulti;
	DWORD GraphicsMode = GetPrivateProfileIntA("Graphics", "Mode", 0, ini);
	if (GraphicsMode != 4 && GraphicsMode != 5) {
		GraphicsMode = 0;
	}
	if (GraphicsMode == 4 || GraphicsMode == 5) {
		dlog("Applying dx9 graphics patch.", DL_INIT);
#ifdef WIN2K
#define _DLL_NAME "d3dx9_42.dll"
#else
#define _DLL_NAME "d3dx9_43.dll"
#endif
		HMODULE h = LoadLibraryEx(_DLL_NAME, 0, LOAD_LIBRARY_AS_DATAFILE);
		if (!h) {
			MessageBoxA(0, "You have selected graphics mode 4 or 5, but " _DLL_NAME " is missing\nSwitch back to mode 0, or install an up to date version of DirectX", "Error", 0);
			ExitProcess(-1);
		} else {
			FreeLibrary(h);
		}
		SafeWrite8(0x0050FB6B, '2');
		dlogr(" Done", DL_INIT);
#undef _DLL_NAME
	}
	fadeMulti = GetPrivateProfileIntA("Graphics", "FadeMultiplier", 100, ini);
	if (fadeMulti != 100) {
		dlog("Applying fade patch.", DL_INIT);
		SafeWrite32(0x00493B17, ((DWORD)&FadeHook) - 0x00493B1b);
		FadeMulti = ((double)fadeMulti) / 100.0;
		dlogr(" Done", DL_INIT);
	}
}

void ApplyWorldmapFpsPatch() {
	DWORD tmp;
	if (GetPrivateProfileInt("Misc", "WorldMapFPSPatch", 0, ini)) {
		dlog("Applying world map fps patch.", DL_INIT);
		if (*(DWORD*)0x004BFE5E != 0x8d16) {
			dlogr(" Failed", DL_INIT);
		} else {
			wp_delay = GetPrivateProfileInt("Misc", "WorldMapDelay2", 66, ini);
			HookCall(0x004BFE5D, worldmap_patch);
			dlogr(" Done", DL_INIT);
		}
	} else {
		tmp = GetPrivateProfileIntA("Misc", "WorldMapFPS", 0, ini);
		if (tmp) {
			dlog("Applying world map fps patch.", DL_INIT);
			if (*((WORD*)0x004CAFB9) == 0x0000) {
				AvailableGlobalScriptTypes |= 2;
				SafeWrite32(0x004BFE5E, ((DWORD)&WorldMapSpeedPatch2) - 0x004BFE62);
				if (GetPrivateProfileIntA("Misc", "ForceLowResolutionTimer", 0, ini) || !QueryPerformanceFrequency((LARGE_INTEGER*)&wm_perfadd) || wm_perfadd <= 1000) {
					wm_wait = 1000.0 / (double)tmp;
					wm_nexttick = GetTickCount();
					wm_usingperf = false;
				} else {
					wm_usingperf = true;
					wm_perfadd /= tmp;
					wm_perfnext = 0;
				}
			}
			dlogr(" Done", DL_INIT);
		} else {
			tmp = GetPrivateProfileIntA("Misc", "WorldMapDelay", 0, ini);
			if (tmp) {
				if (*((WORD*)0x004CAFB9) == 0x3d40)
					SafeWrite32(0x004CAFBB, tmp);
				else if (*((WORD*)0x004CAFB9) == 0x0000) {
					SafeWrite32(0x004BFE5E, ((DWORD)&WorldMapSpeedPatch) - 0x004BFE62);
					WorldMapLoopCount = tmp;
				}
			} else {
				if (*(DWORD*)0x004BFE5E == 0x0000d816) {
					SafeWrite32(0x004BFE5E, ((DWORD)&WorldMapHook) - 0x004BFE62);
				}
			}
		}
		if (GetPrivateProfileIntA("Misc", "WorldMapEncounterFix", 0, ini)) {
			dlog("Applying world map encounter patch.", DL_INIT);
			WorldMapEncounterRate = GetPrivateProfileIntA("Misc", "WorldMapEncounterRate", 5, ini);
			SafeWrite32(0x4C232D, 0x01EBC031);        // xor eax, eax; jmps 0x4C2332
			HookCall(0x4BFEE0, &wmWorldMapFunc_hook);
			MakeCall(0x4C0667, &wmRndEncounterOccurred_hack, false);
			dlogr(" Done", DL_INIT);
		}
	}
}

void ApplyStartingStatePatches() {
	int date = GetPrivateProfileInt("Misc", "StartYear", -1, ini);
	if (date > 0) {
		dlog("Applying starting year patch.", DL_INIT);
		SafeWrite32(0x4A336C, date);
		dlogr(" Done", DL_INIT);
	}
	date = GetPrivateProfileInt("Misc", "StartMonth", -1, ini);
	if (date >= 0 && date < 12) {
		dlog("Applying starting month patch.", DL_INIT);
		SafeWrite32(0x4A3382, date);
		dlogr(" Done", DL_INIT);
	}
	date = GetPrivateProfileInt("Misc", "StartDay", -1, ini);
	if (date >= 0 && date < 31) {
		dlog("Applying starting day patch.", DL_INIT);
		SafeWrite8(0x4A3356, date);
		dlogr(" Done", DL_INIT);
	}

	date = GetPrivateProfileInt("Misc", "StartXPos", -1, ini);
	if (date != -1) {
		dlog("Applying starting x position patch.", DL_INIT);
		SafeWrite32(0x4BC990, date);
		SafeWrite32(0x4BCC08, date);
		dlogr(" Done", DL_INIT);
	}
	date = GetPrivateProfileInt("Misc", "StartYPos", -1, ini);
	if (date != -1) {
		dlog("Applying starting y position patch.", DL_INIT);
		SafeWrite32(0x4BC995, date);
		SafeWrite32(0x4BCC0D, date);
		dlogr(" Done", DL_INIT);
	}

	ViewportX = GetPrivateProfileInt("Misc", "ViewXPos", -1, ini);
	if (ViewportX != -1) {
		dlog("Applying starting x view patch.", DL_INIT);
		SafeWrite32(VARPTR_wmWorldOffsetX, ViewportX);
		HookCall(0x4BCF07, &ViewportHook);
		dlogr(" Done", DL_INIT);
	}
	ViewportY = GetPrivateProfileInt("Misc", "ViewYPos", -1, ini);
	if (ViewportY != -1) {
		dlog("Applying starting y view patch.", DL_INIT);
		SafeWrite32(VARPTR_wmWorldOffsetY, ViewportY);
		HookCall(0x4BCF07, &ViewportHook);
		dlogr(" Done", DL_INIT);
	}
}

void ApplyPlayerModelPatches() {
	StartMaleModelName[64] = 0;
	if (GetPrivateProfileString("Misc", "MaleStartModel", "", StartMaleModelName, 64, ini)) {
		dlog("Applying male start model patch.", DL_INIT);
		SafeWrite32(0x00418B88, (DWORD)&StartMaleModelName);
		dlogr(" Done", DL_INIT);
	}

	StartFemaleModelName[64] = 0;
	if (GetPrivateProfileString("Misc", "FemaleStartModel", "", StartFemaleModelName, 64, ini)) {
		dlog("Applying female start model patch.", DL_INIT);
		SafeWrite32(0x00418BAB, (DWORD)&StartFemaleModelName);
		dlogr(" Done", DL_INIT);
	}

	DefaultMaleModelName[64] = 0;
	GetPrivateProfileString("Misc", "MaleDefaultModel", "hmjmps", DefaultMaleModelName, 64, ini);
	dlog("Applying male model patch.", DL_INIT);
	SafeWrite32(0x00418B50, (DWORD)&DefaultMaleModelName);
	dlogr(" Done", DL_INIT);

	DefaultFemaleModelName[64] = 0;
	GetPrivateProfileString("Misc", "FemaleDefaultModel", "hfjmps", DefaultFemaleModelName, 64, ini);
	dlog("Applying female model patch.", DL_INIT);
	SafeWrite32(0x00418B6D, (DWORD)&DefaultFemaleModelName);
	dlogr(" Done", DL_INIT);
}

void ApplyWorldLimitsPatches() {
	DWORD date = GetPrivateProfileInt("Misc", "LocalMapXLimit", 0, ini);
	if (date) {
		dlog("Applying local map x limit patch.", DL_INIT);
		SafeWrite32(0x004B13B9, date);
		dlogr(" Done", DL_INIT);
	}
	date = GetPrivateProfileInt("Misc", "LocalMapYLimit", 0, ini);
	if (date) {
		dlog("Applying local map y limit patch.", DL_INIT);
		SafeWrite32(0x004B13C7, date);
		dlogr(" Done", DL_INIT);
	}

	//if(GetPrivateProfileIntA("Misc", "WorldMapCitiesListFix", 0, ini)) {
	dlog("Applying world map cities list patch.", DL_INIT);

	SafeWrite32(0x004C04BA, ((DWORD)&ScrollCityListHook) - 0x004C04BE);
	SafeWrite32(0x004C04C9, ((DWORD)&ScrollCityListHook) - 0x004C04CD);
	SafeWrite32(0x004C4A35, ((DWORD)&ScrollCityListHook) - 0x004C4A39);
	SafeWrite32(0x004C4A3E, ((DWORD)&ScrollCityListHook) - 0x004C4A42);
	dlogr(" Done", DL_INIT);
	//}

	//if(GetPrivateProfileIntA("Misc", "CitiesLimitFix", 0, ini)) {
	dlog("Applying cities limit patch.", DL_INIT);
	if (*((BYTE*)0x004BF3BB) != 0xeb) {
		SafeWrite8(0x004BF3BB, 0xeb);
	}
	dlogr(" Done", DL_INIT);
	//}

	DWORD wmSlots = GetPrivateProfileIntA("Misc", "WorldMapSlots", 0, ini);
	if (wmSlots && wmSlots < 128) {
		dlog("Applying world map slots patch.", DL_INIT);
		if (wmSlots < 7) wmSlots = 7;
		mapSlotsScrollMax = (wmSlots - 7) * 27;
		if (wmSlots < 25) SafeWrite32(0x004C21FD, 230 - (wmSlots - 17) * 27);
		else {
			SafeWrite8(0x004C21FC, 0xC2);
			SafeWrite32(0x004C21FD, 2 + 27 * (wmSlots - 26));
		}
		dlogr(" Done", DL_INIT);
	}
}

void ApplyTimeLimitPatch() {
	int limit = GetPrivateProfileIntA("Misc", "TimeLimit", 13, ini);
	if (limit == -2) {
		limit = 14;
	}
	if (limit == -3) {
		dlog("Applying time limit patch (-3).", DL_INIT);
		limit = -1;
		AddUnarmedStatToGetYear = 1;

		SafeWrite32(0x004392F9, ((DWORD)&GetDateWrapper) - 0x004392Fd);
		SafeWrite32(0x00443809, ((DWORD)&GetDateWrapper) - 0x0044380d);
		SafeWrite32(0x0047E128, ((DWORD)&GetDateWrapper) - 0x0047E12c);
		SafeWrite32(0x004975A3, ((DWORD)&GetDateWrapper) - 0x004975A7);
		SafeWrite32(0x00497713, ((DWORD)&GetDateWrapper) - 0x00497717);
		SafeWrite32(0x004979Ca, ((DWORD)&GetDateWrapper) - 0x004979Ce);
		SafeWrite32(0x004C3CB6, ((DWORD)&GetDateWrapper) - 0x004C3CBa);
		dlogr(" Done", DL_INIT);
	}

	if (limit <= 14 && limit >= -1 && limit != 13) {
		dlog("Applying time limit patch.", DL_INIT);
		if (limit == -1) {
			SafeWrite32(0x004A34Fa, ((DWORD)&TimerReset) - 0x004A34Fe);
			SafeWrite32(0x004A3552, ((DWORD)&TimerReset) - 0x004A3556);

			SafeWrite32(0x004A34EF, 0x90909090);
			SafeWrite32(0x004A34f3, 0x90909090);
			SafeWrite16(0x004A34f7, 0x9090);
			SafeWrite32(0x004A34FE, 0x90909090);
			SafeWrite16(0x004A3502, 0x9090);

			SafeWrite32(0x004A3547, 0x90909090);
			SafeWrite32(0x004A354b, 0x90909090);
			SafeWrite16(0x004A354f, 0x9090);
			SafeWrite32(0x004A3556, 0x90909090);
			SafeWrite16(0x004A355a, 0x9090);
		} else {
			SafeWrite8(0x004A34EC, limit);
			SafeWrite8(0x004A3544, limit);
		}
		dlogr(" Done", DL_INIT);
	}
}

void ApplyDebugModePatch() {
	if (IsDebug) {
		DWORD dbgMode = GetPrivateProfileIntA("Debugging", "DebugMode", 0, ".\\ddraw.ini");
		if (dbgMode) {
			dlog("Applying debugmode patch.", DL_INIT);
			//If the player is using an exe with the debug patch already applied, just skip this block without erroring
			if (*((DWORD*)0x00444A64) != 0x082327e8) {
				SafeWrite32(0x00444A64, 0x082327e8);
				SafeWrite32(0x00444A68, 0x0120e900);
				SafeWrite8(0x00444A6D, 0);
				SafeWrite32(0x00444A6E, 0x90909090);
			}
			SafeWrite8(0x004C6D9B, 0xb8);
			if (dbgMode == 1) {
				SafeWrite32(0x004C6D9C, (DWORD)debugGnw);
			}
			else {
				SafeWrite32(0x004C6D9C, (DWORD)debugLog);
			}
			dlogr(" Done", DL_INIT);
		}
	}
}

void ApplyNPCAutoLevelPatch() {
	NpcAutoLevelEnabled = GetPrivateProfileIntA("Misc", "NPCAutoLevel", 0, ini) != 0;
	if (NpcAutoLevelEnabled) {
		dlog("Applying npc autolevel patch.", DL_INIT);
		SafeWrite16(0x00495D22, 0x9090);
		SafeWrite32(0x00495D24, 0x90909090);
		dlogr(" Done", DL_INIT);
	}

	if (GetPrivateProfileIntA("Misc", "SingleCore", 1, ini)) {
		dlog("Applying single core patch.", DL_INIT);
		HANDLE process = GetCurrentProcess();
		SetProcessAffinityMask(process, 1);
		CloseHandle(process);
		dlogr(" Done", DL_INIT);
	}

	if (GetPrivateProfileIntA("Misc", "OverrideArtCacheSize", 0, ini)) {
		dlog("Applying override art cache size patch.", DL_INIT);
		SafeWrite32(0x00418867, 0x90909090);
		SafeWrite32(0x00418872, 256);
		dlogr(" Done", DL_INIT);
	}
}

void ApplyAdditionalWeaponAnimsPatch() {
	if (GetPrivateProfileIntA("Misc", "AdditionalWeaponAnims", 0, ini)) {
		dlog("Applying additional weapon animations patch.", DL_INIT);
		SafeWrite8(0x419320, 0x12);
		HookCall(0x4194CC, WeaponAnimHook);
		HookCall(0x451648, WeaponAnimHook);
		HookCall(0x451671, WeaponAnimHook);
		dlogr(" Done", DL_INIT);
	}
}

void ApplySkilldexImagesPatch() {
	DWORD tmp;
	dlog("Checking for changed skilldex images.", DL_INIT);
	tmp = GetPrivateProfileIntA("Misc", "Lockpick", 293, ini);
	if (tmp != 293) {
		SafeWrite32(0x00518D54, tmp);
	}
	tmp = GetPrivateProfileIntA("Misc", "Steal", 293, ini);
	if (tmp != 293) {
		SafeWrite32(0x00518D58, tmp);
	}
	tmp = GetPrivateProfileIntA("Misc", "Traps", 293, ini);
	if (tmp != 293) {
		SafeWrite32(0x00518D5C, tmp);
	}
	tmp = GetPrivateProfileIntA("Misc", "FirstAid", 293, ini);
	if (tmp != 293) {
		SafeWrite32(0x00518D4C, tmp);
	}
	tmp = GetPrivateProfileIntA("Misc", "Doctor", 293, ini);
	if (tmp != 293) {
		SafeWrite32(0x00518D50, tmp);
	}
	tmp = GetPrivateProfileIntA("Misc", "Science", 293, ini);
	if (tmp != 293) {
		SafeWrite32(0x00518D60, tmp);
	}
	tmp = GetPrivateProfileIntA("Misc", "Repair", 293, ini);
	if (tmp != 293) {
		SafeWrite32(0x00518D64, tmp);
	}
	dlogr(" Done", DL_INIT);
}

void ApplyKarmaFRMsPatch() {
	KarmaFrmCount = GetPrivateProfileIntA("Misc", "KarmaFRMsCount", 0, ini);
	if (KarmaFrmCount) {
		KarmaFrms = new DWORD[KarmaFrmCount];
		KarmaPoints = new int[KarmaFrmCount - 1];
		dlog("Applying karma frm patch.", DL_INIT);
		char buf[512];
		GetPrivateProfileStringA("Misc", "KarmaFRMs", "", buf, 512, ini);
		char *ptr = buf, *ptr2;
		for (DWORD i = 0; i < KarmaFrmCount - 1; i++) {
			ptr2 = strchr(ptr, ',');
			*ptr2 = '\0';
			KarmaFrms[i] = atoi(ptr);
			ptr = ptr2 + 1;
		}
		KarmaFrms[KarmaFrmCount - 1] = atoi(ptr);
		GetPrivateProfileStringA("Misc", "KarmaPoints", "", buf, 512, ini);
		ptr = buf;
		for (DWORD i = 0; i < KarmaFrmCount - 2; i++) {
			ptr2 = strchr(ptr, ',');
			*ptr2 = '\0';
			KarmaPoints[i] = atoi(ptr);
			ptr = ptr2 + 1;
		}
		KarmaPoints[KarmaFrmCount - 2] = atoi(ptr);
		HookCall(0x4367A9, DrawCardHook);
		dlogr(" Done", DL_INIT);
	}
}

void ApplySpeedInterfaceCounterAnimsPatch() {
	switch (GetPrivateProfileIntA("Misc", "SpeedInterfaceCounterAnims", 0, ini)) {
	case 1:
		dlog("Applying SpeedInterfaceCounterAnims patch.", DL_INIT);
		MakeCall(0x460BA1, &intface_rotate_numbers_hack, true);
		dlogr(" Done", DL_INIT);
		break;
	case 2:
		dlog("Applying SpeedInterfaceCounterAnims patch. (Instant)", DL_INIT);
		SafeWrite32(0x460BB6, 0x90DB3190);
		dlogr(" Done", DL_INIT);
		break;
	}
}

void ApplyScienceOnCrittersPatch() {
	switch (GetPrivateProfileIntA("Misc", "ScienceOnCritters", 0, ini)) {
	case 1:
		HookCall(0x41276E, ScienceCritterCheckHook);
		break;
	case 2:
		SafeWrite8(0x41276A, 0xeb);
		break;
	}
}

void ApplyFashShotTraitFix() {
	switch (GetPrivateProfileIntA("Misc", "FastShotFix", 1, ini)) {
	case 1:
		dlog("Applying Fast Shot Trait Fix.", DL_INIT);
		MakeCall(0x478E75, &FastShotTraitFix, true);
		dlogr(" Done", DL_INIT);
		break;
	case 2:
		dlog("Applying Fast Shot Trait Fix. (Fallout 1 version)", DL_INIT);
		SafeWrite16(0x478C9F, 0x9090);
		for (int i = 0; i < sizeof(FastShotFixF1) / 4; i++) {
			HookCall(FastShotFixF1[i], (void*)0x478C7D);
		}
		dlogr(" Done", DL_INIT);
		break;
	}
}

void ApplyBoostScriptDialogLimitPatch() {
	if (GetPrivateProfileIntA("Misc", "BoostScriptDialogLimit", 0, ini)) {
		const int scriptDialogCount = 10000;
		dlog("Applying script dialog limit patch.", DL_INIT);
		scriptDialog = new int[scriptDialogCount * 2]; // Because the msg structure is 8 bytes, not 4.
		SafeWrite32(0x4A50E3, scriptDialogCount); // scr_init
		SafeWrite32(0x4A519F, scriptDialogCount); // scr_game_init
		SafeWrite32(0x4A534F, scriptDialogCount * 8); // scr_message_free
		for (int i = 0; i < sizeof(script_dialog_msgs) / 4; i++) {
			SafeWrite32(script_dialog_msgs[i], (DWORD)scriptDialog); // scr_get_dialog_msg_file
		}
		dlogr(" Done", DL_INIT);
	}
}

void ApplyPathfinderFix() {
	//if(GetPrivateProfileIntA("Misc", "PathfinderFix", 0, ini)) {
	dlog("Applying pathfinder patch.", DL_INIT);
	SafeWrite32(0x004C1FF2, ((DWORD)&PathfinderFix3) - 0x004c1ff6);
	SafeWrite32(0x004C1C79, ((DWORD)&PathfinderFix) - 0x004c1c7d);
	MapMulti = (double)GetPrivateProfileIntA("Misc", "WorldMapTimeMod", 100, ini) / 100.0;
	dlogr(" Done", DL_INIT);
//}
}

void ApplyNumbersInDialoguePatch() {
	if (GetPrivateProfileIntA("Misc", "NumbersInDialogue", 0, ini)) {
		dlog("Applying numbers in dialogue patch.", DL_INIT);
		SafeWrite32(0x502C32, 0x2000202E);
		SafeWrite8(0x446F3B, 0x35);
		SafeWrite32(0x5029E2, 0x7325202E);
		SafeWrite32(0x446F03, 0x2424448B);        // mov  eax, [esp+0x24]
		SafeWrite8(0x446F07, 0x50);               // push eax
		SafeWrite32(0x446FE0, 0x2824448B);        // mov  eax, [esp+0x28]
		SafeWrite8(0x446FE4, 0x50);               // push eax
		MakeCall(0x4458F5, &gdAddOptionStr_hack, true);
		dlogr(" Done", DL_INIT);
	}
}

void ApplyInstantWeaponEquipPatch() {
	if (GetPrivateProfileIntA("Misc", "InstantWeaponEquip", 0, ini)) {
		//Skip weapon equip/unequip animations
		dlog("Applying instant weapon equip patch.", DL_INIT);
		for (int i = 0; i < sizeof(PutAwayWeapon) / 4; i++) {
			SafeWrite8(PutAwayWeapon[i], 0xEB);   // jmps
		}
		BlockCall(0x472AD5);                      //
		BlockCall(0x472AE0);                      // invenUnwieldFunc_
		BlockCall(0x472AF0);                      //
		MakeCall(0x415238, &register_object_take_out_hack, true);
		dlogr(" Done", DL_INIT);
	}
}

void ApplyCombatProcFix() {
	//Ray's combat_p_proc fix
		SafeWrite32(0x0425253, ((DWORD)&Combat_p_procFix) - 0x0425257);
		SafeWrite8(0x0424dbc, 0xE9);
		SafeWrite32(0x0424dbd, 0x00000034);
		dlogr(" Done", DL_INIT);
	//}
}

void ApplyDataLoadOrderPatch() {
	if (GetPrivateProfileIntA("Misc", "DataLoadOrderPatch", 0, ini)) {
		dlog("Applying data load order patch.", DL_INIT);
		MakeCall(0x444259, &game_init_databases_hack1, false);
		MakeCall(0x4442F1, &game_init_databases_hack2, false);
		HookCall(0x44436D, &game_init_databases_hook);
		SafeWrite8(0x4DFAEC, 0x1D); // error correction
		dlogr(" Done", DL_INIT);
	}
}

void ApplyDisplayKarmaChangesPatch() {
	if (GetPrivateProfileInt("Misc", "DisplayKarmaChanges", 0, ini)) {
		dlog("Applying display karma changes patch.", DL_INIT);
		GetPrivateProfileString("sfall", "KarmaGain", "You gained %d karma.", KarmaGainMsg, 128, translationIni);
		GetPrivateProfileString("sfall", "KarmaLoss", "You lost %d karma.", KarmaLossMsg, 128, translationIni);
		HookCall(0x455A6D, SetGlobalVarWrapper);
		dlogr(" Done", DL_INIT);
	}
}

void ApplyMultiPatchesPatch() {
	if (GetPrivateProfileIntA("Misc", "MultiPatches", 0, ini)) {
		dlog("Applying load multiple patches patch.", DL_INIT);
		SafeWrite8(0x444354, 0x90); // Change step from 2 to 1
		SafeWrite8(0x44435C, 0xC4); // Disable check
		dlogr(" Done", DL_INIT);
	}
}

void ApplyPlayIdleAnimOnReloadPatch() {
	if (GetPrivateProfileInt("Misc", "PlayIdleAnimOnReload", 0, ini)) {
		dlog("Applying idle anim on reload patch.", DL_INIT);
		HookCall(0x460B8C, ReloadHook);
		dlogr(" Done", DL_INIT);
	}
}

void ApplyCorpseLineOfFireFix() {
	if (GetPrivateProfileInt("Misc", "CorpseLineOfFireFix", 0, ini)) {
		dlog("Applying corpse line of fire patch.", DL_INIT);
		MakeCall(0x48B994, CorpseHitFix2, true);
		MakeCall(0x48BA04, CorpseHitFix2b, true);
		dlogr(" Done", DL_INIT);
	}
}

void ApplyNpcExtraApPatch() {
	RetryCombatMinAP = GetPrivateProfileIntA("Misc", "NPCsTryToSpendExtraAP", 0, ini);
	if (RetryCombatMinAP > 0) {
		dlog("Applying retry combat patch.", DL_INIT);
		HookCall(0x422B94, &RetryCombatHook);
		dlogr(" Done", DL_INIT);
	}
}

void ApplyNpcStage6Fix() {
	if (GetPrivateProfileIntA("Misc", "NPCStage6Fix", 0, ini)) {
		dlog("Applying NPC Stage 6 Fix.", DL_INIT);
		MakeCall(0x493CE9, &NPCStage6Fix1, true);
		SafeWrite8(0x494063, 0x06);		// loop should look for a potential 6th stage
		SafeWrite8(0x4940BB, 0xCC);		// move pointer by 204 bytes instead of 200
		MakeCall(0x494224, &NPCStage6Fix2, true);
		dlogr(" Done", DL_INIT);
	}
}

void ApplyMotionScannerFlagsPatch() {
	DWORD flags;
	if (flags = GetPrivateProfileIntA("Misc", "MotionScannerFlags", 1, ini)) {
		dlog("Applying MotionScannerFlags patch.", DL_INIT);
		if (flags & 1) MakeCall(0x41BBE9, &ScannerAutomapHook, true);
		if (flags & 2) BlockCall(0x41BC3C);
		dlogr(" Done", DL_INIT);
	}
}

void ApplyEncounterTableSizePatch() {
	DWORD tableSize = GetPrivateProfileIntA("Misc", "EncounterTableSize", 0, ini);
	if (tableSize > 40 && tableSize <= 127) {
		dlog("Applying EncounterTableSize patch.", DL_INIT);
		SafeWrite8(0x4BDB17, (BYTE)tableSize);
		DWORD nsize = (tableSize + 1) * 180 + 0x50;
		for (int i = 0; i < sizeof(EncounterTableSize) / 4; i++) {
			SafeWrite32(EncounterTableSize[i], nsize);
		}
		SafeWrite8(0x4BDB17, (BYTE)tableSize);
		dlogr(" Done", DL_INIT);
	}
}

void ApplyObjCanSeeShootThroughPatch() {
	if (GetPrivateProfileIntA("Misc", "ObjCanSeeObj_ShootThru_Fix", 0, ini)) {
		dlog("Applying ObjCanSeeObj ShootThru Fix.", DL_INIT);
		SafeWrite32(0x456BC7, (DWORD)&objCanSeeObj_ShootThru_Fix - 0x456BCB);
		dlogr(" Done", DL_INIT);
	}
}

void ApplyTownMapsHotkeyFix() {
	if (GetPrivateProfileIntA("Misc", "TownMapHotkeysFix", 1, ini)) {
		dlog("Applying town map hotkeys patch.", DL_INIT);
		MakeCall(0x4C4945, &wmTownMapFunc_hack, false);
		dlogr(" Done", DL_INIT);
	}
}

static const char* musicOverridePath = "data\\sound\\music\\";
void ApplyOverrideMusicDirPatch() {
	DWORD overrideMode;
	if (overrideMode = GetPrivateProfileIntA("Sound", "OverrideMusicDir", 0, ini)) {
		SafeWrite32(0x4449C2, (DWORD)musicOverridePath);
		SafeWrite32(0x4449DB, (DWORD)musicOverridePath);
		if (overrideMode == 2) {
			SafeWrite32(0x518E78, (DWORD)musicOverridePath);
			SafeWrite32(0x518E7C, (DWORD)musicOverridePath);
		}
	}
}

void ApplyMiscPatches() {
	mapName[64] = 0;
	if (GetPrivateProfileString("Misc", "StartingMap", "", mapName, 64, ini)) {
		dlog("Applying starting map patch.", DL_INIT);
		SafeWrite32(0x00480AAA, (DWORD)&mapName);
		dlogr(" Done", DL_INIT);
	}

	versionString[64] = 0;
	if (GetPrivateProfileString("Misc", "VersionString", "", versionString, 64, ini)) {
		dlog("Applying version string patch.", DL_INIT);
		SafeWrite32(0x004B4588, (DWORD)&versionString);
		dlogr(" Done", DL_INIT);
	}

	configName[64] = 0;
	if (GetPrivateProfileString("Misc", "ConfigFile", "", configName, 64, ini)) {
		dlog("Applying config file patch.", DL_INIT);
		SafeWrite32(0x00444BA5, (DWORD)&configName);
		SafeWrite32(0x00444BCA, (DWORD)&configName);
		dlogr(" Done", DL_INIT);
	}

	patchName[64] = 0;
	if (GetPrivateProfileString("Misc", "PatchFile", "", patchName, 64, ini)) {
		dlog("Applying patch file patch.", DL_INIT);
		SafeWrite32(0x00444323, (DWORD)&patchName);
		dlogr(" Done", DL_INIT);
	}
}

void MiscReset() {
	if (scriptDialog != nullptr) {
		delete[] scriptDialog;
	}
}