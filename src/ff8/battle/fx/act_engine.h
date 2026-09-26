/****************************************************************************/
//    Copyright (C) 2026 HobbitDur                                          //
//                                                                          //
//    This file is part of FFNx                                             //
//                                                                          //
//    FFNx is free software: you can redistribute it and/or modify          //
//    it under the terms of the GNU General Public License as published by  //
//    the Free Software Foundation, either version 3 of the License         //
//                                                                          //
//    FFNx is distributed in the hope that it will be useful,               //
//    but WITHOUT ANY WARRANTY; without even the implied warranty of        //
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         //
//    GNU General Public License for more details.                          //
/****************************************************************************/

// Actor state-machine GF family ("Cure engine" family, code 0x70D000-0x768000): native port of
// the effect library that Siren (095), MiniMog (096), Tonberry (090) and the four Boko summons
// (097-100) are built from - and, beyond the GFs, most heal/support spells (Cure family).
//
// Like the GF cinematic engine, the library is ONE source compiled into every module: each GF
// module is a private copy of the library functions it uses, reading its OWN module globals
// (task pools, script cursors, arena pointers, asset tables). The port is written once:
//   * an "engine" port (a_XXXXXX, XXXXXX = canonical address = the Siren copy when Siren has one)
//     is the twin of a function whose code is identical in at least two modules (identical up to
//     module data addresses and stored code pointers; callees compared by their own content);
//   * module data and code addresses it uses go through the module descriptor: G(G_xxx) /
//     F(F_xxx) = the address of the canonical global / function xxx in the CURRENT module
//     (g_mod, set by the module's master task and by the held-frame draw);
//   * functions unique to a module are ported in that module's file (s_XXXXXX Siren,
//     m_XXXXXX MiniMog) with raw addresses.
// Every port has the original cdecl shape with 32-bit words for arguments and return value, so
// a port and the original are interchangeable through a function pointer: `callp(addr, ...)`
// calls the port of an original address when there is one (state tables hold original
// addresses, exactly like the original stack tables), `callo(addr, ...)` the original.
//
// Instantiating another module (Tonberry, Boko): its descriptor (MOD_090 ... MOD_100) and the
// list of its addresses served by engine ports (ENGINE_PORTS_xxx) are generated already (see
// act_engine.cpp); port its unique functions like mag095_siren.cpp does and call
// register_module(effect_id) from its register function.

#pragma once

#include "fx_port.h"

namespace ff8fx
{
namespace act
{
	using namespace ff8fx::eng;

	// ------------------------------------------------------------------------------------
	// raw memory access (the ports follow the listings offset by offset)
	// ------------------------------------------------------------------------------------
	template<typename T> inline T &MEM(uint32_t addr) { return *(T *)addr; }
	template<typename T> inline T &AT(uint32_t p, int32_t off) { return *(T *)(p + off); }
	inline int8_t &S8(uint32_t p, int32_t o) { return AT<int8_t>(p, o); }
	inline uint8_t &U8(uint32_t p, int32_t o) { return AT<uint8_t>(p, o); }
	inline int16_t &S16(uint32_t p, int32_t o) { return AT<int16_t>(p, o); }
	inline uint16_t &U16(uint32_t p, int32_t o) { return AT<uint16_t>(p, o); }
	inline int32_t &S32(uint32_t p, int32_t o) { return AT<int32_t>(p, o); }
	inline uint32_t &U32(uint32_t p, int32_t o) { return AT<uint32_t>(p, o); }
	inline int32_t add32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
	inline int32_t sub32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }
	inline uint32_t P(const void *p) { return (uint32_t)p; }  // pointer -> 32-bit word

	// ---- generated: cross-module address classes (canonical = Siren address when Siren has it) ----
	enum GIdx : int
	{
		G_152BBA0,
		G_1532F80,
		G_1533010,
		G_15339D0,
		G_1533D18,
		G_1D969A8,
		G_257B748,
		G_257F4D4,
		G_257F698,
		G_257F720,
		G_257F728,
		G_257F730,
		G_257F8A0,
		G_257F8A4,
		G_257F988,
		G_257F9AC,
		G_257F9B0,
		G_257FA90,
		G_2585AE0,
		G_2585E38,
		G_2585EF0,
		G_258A010,
		G_258A020,
		G_258A024,
		G_258BA90,
		G_258BE20,
		G_258BE30,
		G_258BFA0,
		G_258BFA4,
		G_258BFA8,
		G_258EAA0,
		G_258EC40,
		G_258EC50,
		G_258EC54,
		G_258EF98,
		G_258FB40,
		G_258FB48,
		G_258FB68,
		G_258FB70,
		G_258FB78,
		NG
	};
	enum FIdx : int
	{
		F_732160,
		F_732AC0,
		F_7332C0,
		F_733310,
		F_733340,
		F_733370,
		F_7333A0,
		F_7333D0,
		F_733400,
		F_733430,
		F_733460,
		F_733490,
		F_7334C0,
		F_7334F0,
		F_733510,
		F_733530,
		F_733560,
		F_7348D0,
		F_7353B0,
		F_735440,
		F_7354A0,
		F_7355E0,
		F_735720,
		F_735930,
		F_735BB0,
		F_735DF0,
		F_736090,
		F_7362C0,
		F_736580,
		F_7367E0,
		F_736AC0,
		F_736C00,
		F_736C40,
		F_737ED0,
		F_737F20,
		F_737F50,
		F_737FD0,
		F_7386C0,
		F_738710,
		F_7387B0,
		F_738870,
		F_7390C0,
		F_7394C0,
		F_739550,
		F_739960,
		F_73A0D0,
		F_73A0E0,
		F_73A0F0,
		F_73A170,
		F_73A1A0,
		F_73A220,
		F_73A3E0,
		F_73A580,
		F_73A5B0,
		F_73A5E0,
		F_73A5F0,
		F_73A600,
		F_73A610,
		F_73A620,
		F_73A660,
		F_73A6D0,
		F_73A950,
		F_73AAE0,
		F_73AB20,
		F_73AE10,
		F_73B160,
		F_73B430,
		F_73B520,
		F_73B570,
		F_73B640,
		F_73B770,
		F_73B7E0,
		F_73C3B0,
		F_73F180,
		F_73F1D0,
		F_73F1E0,
		F_73F220,
		F_73F240,
		F_73FB40,
		F_73FC20,
		F_73FDC0,
		F_73FE90,
		F_73FFB0,
		F_740210,
		F_7407C0,
		F_740910,
		F_740F70,
		F_740FC0,
		F_741050,
		F_741120,
		F_7418B0,
		F_7419C0,
		F_741A40,
		F_741A70,
		F_742290,
		F_7422C0,
		F_7423A0,
		F_7424C0,
		F_742610,
		F_742CA0,
		F_742CD0,
		F_742EB0,
		F_742FF0,
		F_743720,
		F_743C20,
		F_747440,
		F_7474B0,
		F_7474D0,
		F_7474F0,
		F_747500,
		F_747540,
		F_747550,
		F_747560,
		F_747590,
		F_7475A0,
		F_7475C0,
		NF
	};

	namespace x
	{
		inline uint32_t sub_45BFC0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)>(0x45BFC0)(a1, a2, a3, a4, a5); }
		inline uint32_t sub_45C690(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x45C690)(a1, a2, a3, a4); }
		inline uint32_t SSIGPU_InsertPrimAutoDepth(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x45C7A0)(a1, a2); }
		inline uint32_t SSIGPU_InsertPrimDepthKeys(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)>(0x45C870)(a1, a2, a3, a4, a5, a6); }
		inline uint32_t SSIGPU_InsertPrimAltViewport(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x45C8E0)(a1, a2); }
		inline uint32_t someCameraWork_45DD60(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DD60)(a1, a2, a3); }
		inline uint32_t GTE_SetRotMatrix(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45DE00)(a1); }
		inline uint32_t GTE_SetTransVector(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45DEF0)(a1); }
		inline uint32_t GTE_LoadV0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45DF80)(a1); }
		inline uint32_t GTE_LoadV012(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45DFE0)(a1, a2, a3); }
		inline uint32_t GTE_LoadV0FromDwords(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E060)(a1); }
		inline uint32_t set_unk_1CA8A28(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E110)(a1); }
		inline uint32_t sub_45E120(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E120)(a1, a2, a3); }
		inline uint32_t set_dword_1CA8A30(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E150)(a1); }
		inline uint32_t GTE_LoadIRFromMatrixColumn(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E180)(a1); }
		inline uint32_t GTE_ReadFLAG(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E210)(a1); }
		inline uint32_t GTE_ReadSXY2(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E260)(a1); }
		inline uint32_t GTE_ReadSXY012_Split(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E270)(a1, a2, a3); }
		inline uint32_t GTE_StoreSXY012_PolyGT3_2(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E340)(a1); }
		inline uint32_t set_param_with_dword_1CA8A68(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E360)(a1); }
		inline uint32_t sub_45E370(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x45E370)(a1, a2, a3); }
		inline uint32_t GTE_ReadMAC0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E3C0)(a1); }
		inline uint32_t GTE_ReadOTZ(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E3D0)(a1); }
		inline uint32_t GTE_StoreIR123(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E3E0)(a1); }
		inline uint32_t GTE_ReadMAC123(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E450)(a1); }
		inline uint32_t GTE_StoreIRToMatrixColumn(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E470)(a1); }
		inline uint32_t CopyCameraStateToBuffer(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x45E500)(a1); }
		inline uint32_t GTE_AVSZ3() { return fn<uint32_t (__cdecl *)()>(0x45E5C0)(); }
		inline uint32_t GTE_AVSZ4() { return fn<uint32_t (__cdecl *)()>(0x45E610)(); }
		inline uint32_t GTE_NCLIP() { return fn<uint32_t (__cdecl *)()>(0x45EE10)(); }
		inline uint32_t sub_45F270() { return fn<uint32_t (__cdecl *)()>(0x45F270)(); }
		inline uint32_t sub_45F4C0() { return fn<uint32_t (__cdecl *)()>(0x45F4C0)(); }
		inline uint32_t GTE_RTPS() { return fn<uint32_t (__cdecl *)()>(0x45FAA0)(); }
		inline uint32_t GTE_RTPT() { return fn<uint32_t (__cdecl *)()>(0x45FE10)(); }
		inline uint32_t GTE_MVMVA_RotIR() { return fn<uint32_t (__cdecl *)()>(0x460820)(); }
		inline uint32_t GTE_MVMVA_RotV0_Tr_2() { return fn<uint32_t (__cdecl *)()>(0x460830)(); }
		inline uint32_t GTE_MVMVA_RotV0_Tr() { return fn<uint32_t (__cdecl *)()>(0x460840)(); }
		inline uint32_t GTE_MVMVA_RotV0() { return fn<uint32_t (__cdecl *)()>(0x460850)(); }
		inline uint32_t Music_SetVolumeImmediate(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x46BB40)(a1, a2); }
		inline uint32_t pre_LoadBattleFile(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x48D0A0)(a1, a2, a3, a4); }
		inline uint32_t sub_4A2940(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x4A2940)(a1); }
		inline uint32_t BdSound_ClaimVoiceSlot(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x4A29A0)(a1, a2, a3); }
		inline uint32_t BdPlaySE(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x501330)(a1, a2, a3); }
		inline uint32_t BdTransSummonStream(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x501860)(a1, a2); }
		inline uint32_t BdPlaySummonStream(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x5018C0)(a1, a2, a3); }
		inline uint32_t GetEffectSpawnPosition(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x502170)(a1, a2, a3, a4); }
		inline uint32_t queueChainTransformationConditional(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x505CE0)(a1, a2); }
		inline uint32_t Battle_QueueTIMUpload_GetEOF(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x505E30)(a1); }
		inline uint32_t QueueBlitCommand(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x505EB0)(a1, a2, a3); }
		inline uint32_t ApplyActionResultToTarget(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x506690)(a1); }
		inline uint32_t Field_Alloc(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x5082B0)(a1); }
		inline uint32_t Field_Free(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x5082D0)(a1); }
		inline uint32_t BdLink_InitTaskQueuePool(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x508300)(a1, a2, a3, a4); }
		inline uint32_t ExecuteTaskQueue(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x508420)(a1); }
		inline uint32_t nullsub_1029() { return fn<uint32_t (__cdecl *)()>(0x5084E0)(); }
		inline uint32_t sub_508630(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x508630)(a1, a2); }
		inline uint32_t sub_5086F0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x5086F0)(a1); }
		inline uint32_t sub_5088A0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x5088A0)(a1, a2, a3, a4); }
		inline uint32_t BattleModel_BuildBoneMatricesFromPose(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x508C90)(a1); }
		inline uint32_t BS_ComputeBonesWorldMatrices(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x5095B0)(a1, a2); }
		inline uint32_t RenderGeometry(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)>(0x5099D0)(a1, a2, a3, a4, a5); }
		inline uint32_t CrtRand() { return fn<uint32_t (__cdecl *)()>(0x55CBD2)(); }
		inline uint32_t GTE_SetRotMatrix_W(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x56BAE0)(a1); }
		inline uint32_t GTE_SetTransVector_W(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x56BB30)(a1); }
		inline uint32_t Sqrt(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x56BEC0)(a1); }
		inline uint32_t scale3DMatrix(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x56BEF0)(a1, a2); }
		inline uint32_t ScaleMatrix3x3(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x56BF80)(a1, a2); }
		inline uint32_t UnpackRotationMatrix(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x56C040)(a1, a2); }
		inline uint32_t sub_56C220(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x56C220)(a1, a2); }
		inline uint32_t GTE_MatrixMultiply(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x56C270)(a1, a2); }
		inline uint32_t ComposeAffineTransform(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x56C2F0)(a1, a2, a3); }
		inline uint32_t matrixMultiplyVector(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x56C4F0)(a1, a2, a3); }
		inline uint32_t TransformVectorBy3x3Matrix(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x56C600)(a1, a2, a3); }
		inline uint32_t ComposeZYXRotationMatrix(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x56CE30)(a1, a2); }
		inline uint32_t ApplyZRotation(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x56D090)(a1, a2); }
		inline uint32_t computeCosine(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x56D100)(a1); }
		inline uint32_t computeSin(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x56D130)(a1); }
		inline uint32_t CartesianToGameAngle(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x56D160)(a1, a2); }
		inline uint32_t IO_GetFile_MAGIC(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x571B80)(a1); }
		inline uint32_t Effect_RenderPrimModel(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x572200)(a1, a2, a3, a4); }
		inline uint32_t MAG_017_sub_701310(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x701310)(a1, a2); }
		inline uint32_t MAG_017_sub_701390(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)>(0x701390)(a1, a2, a3, a4, a5); }
		inline uint32_t MAG_117_sub_701430(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x701430)(a1, a2); }
		inline uint32_t Effect_DecodeModelPrimLayout(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x7016B0)(a1, a2, a3); }
		inline uint32_t MAG_011_sub_701970(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x701970)(a1, a2, a3, a4); }
		inline uint32_t Effect_ReleaseLinkedTask(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x8DC530)(a1); }
		inline uint32_t Effect_AddTaskAndInitFromCtx(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x8DC540)(a1, a2, a3, a4); }
		inline uint32_t MAG_001_CURE_Emitter_UpdatePos(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x8DC610)(a1); }
		inline uint32_t Effect_UpdateTargetPosFromBones(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x8DC740)(a1); }
		inline uint32_t MAG_001_CURE_Emitter_ComputeModelBounds(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x8DC870)(a1); }
		inline uint32_t MAG_007_sub_8DCC00(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x8DCC00)(a1, a2); }
		inline uint32_t Effect_BindModelContainerSetAnim(uint32_t a1, uint32_t a2, uint32_t a3) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t)>(0x8DD0F0)(a1, a2, a3); }
		inline uint32_t au_re_Battle_ReadAnimation_7(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x8DD190)(a1, a2); }
		inline uint32_t sub_8DD1C0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x8DD1C0)(a1); }
		inline uint32_t au_re_Battle_ReadAnimation_8(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x8DD220)(a1); }
		inline uint32_t InitEffectSequenceFromData_c19(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t, uint32_t, uint32_t)>(0x8DD260)(a1, a2, a3, a4); }
		inline uint32_t MAG_022_sub_8DD770(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x8DD770)(a1); }
		inline uint32_t sub_8DD7B0(uint32_t a1) { return fn<uint32_t (__cdecl *)(uint32_t)>(0x8DD7B0)(a1); }
		inline uint32_t sub_8DD7E0(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x8DD7E0)(a1, a2); }
		inline uint32_t MAG_022_sub_8DD8A0(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x8DD8A0)(a1, a2); }
		inline uint32_t sub_8DD960(uint32_t a1, uint32_t a2) { return fn<uint32_t (__cdecl *)(uint32_t, uint32_t)>(0x8DD960)(a1, a2); }
	}

	// ---- generated: engine ports (every port takes/returns 32-bit words, cdecl, like the original) ----
	uint32_t __cdecl a_739F40(uint32_t a1); // 0x739F40 GF_095Siren_SequenceTick TASK, 91 insns
	uint32_t __cdecl a_73A0D0(uint32_t a1); // 0x73A0D0 MAG_095_sub_73A0D0, 3 insns
	uint32_t __cdecl a_73A170(uint32_t a1); // 0x73A170 MAG_095_sub_73A170, 14 insns
	uint32_t __cdecl a_73A1A0(uint32_t a1); // 0x73A1A0 MAG_095_sub_73A1A0 TASK, 34 insns
	uint32_t __cdecl a_73A380(uint32_t a1); // 0x73A380 MAG_095_sub_73A380 TASK, 28 insns
	uint32_t __cdecl a_73A3E0(uint32_t a1); // 0x73A3E0 sub_73A3E0, 92 insns
	uint32_t __cdecl a_73A580(void); // 0x73A580 sub_73A580, 8 insns
	uint32_t __cdecl a_73A5B0(void); // 0x73A5B0 sub_73A5B0, 11 insns
	uint32_t __cdecl a_73A5E0(void); // 0x73A5E0 sub_73A5E0, 3 insns
	uint32_t __cdecl a_73A620(void); // 0x73A620 sub_73A620, 13 insns
	uint32_t __cdecl a_73A660(uint32_t a1); // 0x73A660 sub_73A660, 26 insns
	uint32_t __cdecl a_73A720(uint32_t a1); // 0x73A720 sub_73A720, 60 insns
	uint32_t __cdecl a_73A930(uint32_t a1); // 0x73A930 sub_73A930, 6 insns
	uint32_t __cdecl a_73A950(void); // 0x73A950 sub_73A950, 14 insns
	uint32_t __cdecl a_73AAE0(uint32_t a1); // 0x73AAE0 sub_73AAE0, 6 insns
	uint32_t __cdecl a_73AB00(void); // 0x73AB00 sub_73AB00, 6 insns
	uint32_t __cdecl a_73AB20(uint32_t a1, uint32_t a2, uint32_t a3); // 0x73AB20 sub_73AB20, 172 insns
	uint32_t __cdecl a_73AE10(void); // 0x73AE10 sub_73AE10, 220 insns
	uint32_t __cdecl a_73B160(void); // 0x73B160 sub_73B160, 87 insns
	uint32_t __cdecl a_73B2D0(uint32_t a1); // 0x73B2D0 sub_73B2D0, 6 insns
	uint32_t __cdecl a_73B350(uint32_t a1); // 0x73B350 sub_73B350, 15 insns
	uint32_t __cdecl a_73B3B0(uint32_t a1); // 0x73B3B0 sub_73B3B0, 35 insns
	uint32_t __cdecl a_73B430(uint32_t a1, uint32_t a2); // 0x73B430 sub_73B430, 28 insns
	uint32_t __cdecl a_73B4B0(void); // 0x73B4B0 sub_73B4B0, 114 insns
	uint32_t __cdecl a_73B520(uint32_t a1, uint32_t a2); // 0x73B520 sub_73B520, 35 insns
	uint32_t __cdecl a_73B570(uint32_t a1, uint32_t a2); // 0x73B570 sub_73B570, 59 insns
	uint32_t __cdecl a_73B620(uint32_t a1); // 0x73B620 sub_73B620, 8 insns
	uint32_t __cdecl a_73B640(uint32_t a1); // 0x73B640 sub_73B640, 18 insns
	uint32_t __cdecl a_73B750(uint32_t a1); // 0x73B750 sub_73B750, 7 insns
	uint32_t __cdecl a_73A6D0(void); // 0x73A6D0 nullsub_1171, 1 insns
	uint32_t __cdecl a_73B790(uint32_t a1); // 0x73B790 sub_73B790, 8 insns
	uint32_t __cdecl a_73B7E0(uint32_t a1); // 0x73B7E0 sub_73B7E0, 6 insns
	uint32_t __cdecl a_73B8A0(uint32_t a1); // 0x73B8A0 sub_73B8A0, 8 insns
	uint32_t __cdecl a_73B8C0(uint32_t a1); // 0x73B8C0 sub_73B8C0, 8 insns
	uint32_t __cdecl a_73B9F0(void); // 0x73B9F0 sub_73B9F0, 18 insns
	uint32_t __cdecl a_73BA90(void); // 0x73BA90 sub_73BA90, 18 insns
	uint32_t __cdecl a_73BB60(uint32_t a1); // 0x73BB60 sub_73BB60, 22 insns
	uint32_t __cdecl a_73BBD0(uint32_t a1); // 0x73BBD0 sub_73BBD0, 8 insns
	uint32_t __cdecl a_73BC40(uint32_t a1); // 0x73BC40 sub_73BC40, 6 insns
	uint32_t __cdecl a_73BCB0(uint32_t a1); // 0x73BCB0 sub_73BCB0, 11 insns
	uint32_t __cdecl a_73C100(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6); // 0x73C100 sub_73C100, 17 insns
	uint32_t __cdecl a_73C280(uint32_t a1); // 0x73C280 sub_73C280, 82 insns
	uint32_t __cdecl a_73D770(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x73D770 sub_73D770, 191 insns
	uint32_t __cdecl a_73D9C0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x73D9C0 sub_73D9C0, 233 insns
	uint32_t __cdecl a_73F110(uint32_t a1); // 0x73F110 sub_73F110 TASK, 30 insns
	uint32_t __cdecl a_73F990(uint32_t a1); // 0x73F990 sub_73F990, 121 insns
	uint32_t __cdecl a_73FC20(void); // 0x73FC20 sub_73FC20, 100 insns
	uint32_t __cdecl a_73FE90(uint32_t a1); // 0x73FE90 sub_73FE90, 89 insns
	uint32_t __cdecl a_73FFB0(void); // 0x73FFB0 sub_73FFB0, 169 insns
	uint32_t __cdecl a_740210(uint32_t a1); // 0x740210 sub_740210, 154 insns
	uint32_t __cdecl a_740700(void); // 0x740700 sub_740700, 59 insns
	uint32_t __cdecl a_7407C0(uint32_t a1); // 0x7407C0 sub_7407C0, 34 insns
	uint32_t __cdecl a_740880(uint32_t a1); // 0x740880 sub_740880, 46 insns
	uint32_t __cdecl a_740910(uint32_t a1, uint32_t a2); // 0x740910 sub_740910, 459 insns
	uint32_t __cdecl a_740F70(uint32_t a1, uint32_t a2); // 0x740F70 sub_740F70, 27 insns
	uint32_t __cdecl a_740FC0(uint32_t a1, uint32_t a2); // 0x740FC0 sub_740FC0, 53 insns
	uint32_t __cdecl a_741050(uint32_t a1, uint32_t a2); // 0x741050 sub_741050, 53 insns
	uint32_t __cdecl a_741120(uint32_t a1, uint32_t a2); // 0x741120 sub_741120, 518 insns
	uint32_t __cdecl a_7419C0(uint32_t a1, uint32_t a2); // 0x7419C0 sub_7419C0, 39 insns
	uint32_t __cdecl a_741A40(uint32_t a1, uint32_t a2); // 0x741A40 sub_741A40, 12 insns
	uint32_t __cdecl a_741A70(uint32_t a1, uint32_t a2, uint32_t a3); // 0x741A70 sub_741A70, 55 insns
	uint32_t __cdecl a_741B60(uint32_t a1); // 0x741B60 sub_741B60, 14 insns
	uint32_t __cdecl a_742290(uint32_t a1, uint32_t a2); // 0x742290 au_re__rand_8, 22 insns
	uint32_t __cdecl a_7422C0(uint32_t a1, uint32_t a2); // 0x7422C0 sub_7422C0, 27 insns
	uint32_t __cdecl a_742300(uint32_t a1, uint32_t a2, uint32_t a3); // 0x742300 sub_742300, 29 insns
	uint32_t __cdecl a_742350(uint32_t a1, uint32_t a2, uint32_t a3); // 0x742350 sub_742350, 29 insns
	uint32_t __cdecl a_7423A0(uint32_t a1, uint32_t a2); // 0x7423A0 au_re__rand_8_0, 31 insns
	uint32_t __cdecl a_7424B0(uint32_t a1); // 0x7424B0 sub_7424B0, 4 insns
	uint32_t __cdecl a_7424C0(uint32_t a1); // 0x7424C0 sub_7424C0, 107 insns
	uint32_t __cdecl a_742CA0(uint32_t a1, uint32_t a2); // 0x742CA0 sub_742CA0, 15 insns
	uint32_t __cdecl a_742CD0(uint32_t a1); // 0x742CD0 sub_742CD0, 15 insns
	uint32_t __cdecl a_742D00(uint32_t a1); // 0x742D00 sub_742D00, 126 insns
	uint32_t __cdecl a_742EB0(uint32_t a1); // 0x742EB0 sub_742EB0, 90 insns
	uint32_t __cdecl a_743100(uint32_t a1, uint32_t a2); // 0x743100 sub_743100, 46 insns
	uint32_t __cdecl a_743190(void); // 0x743190 sub_743190, 55 insns
	uint32_t __cdecl a_7435E0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x7435E0 InitEffectSequenceFromData_c2, 103 insns
	uint32_t __cdecl a_743720(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x743720 sub_743720, 346 insns
	uint32_t __cdecl a_743C00(uint32_t a1); // 0x743C00 sub_743C00, 13 insns
	uint32_t __cdecl a_743C20(uint32_t a1); // 0x743C20 sub_743C20, 12 insns
	uint32_t __cdecl a_7458E0(uint32_t a1); // 0x7458E0 sub_7458E0, 71 insns
	uint32_t __cdecl a_746980(uint32_t a1); // 0x746980 sub_746980, 18 insns
	uint32_t __cdecl a_746A10(void); // 0x746A10 sub_746A10, 9 insns
	uint32_t __cdecl a_746AE0(void); // 0x746AE0 sub_746AE0, 15 insns
	uint32_t __cdecl a_746C10(uint32_t a1, uint32_t a2, uint32_t a3); // 0x746C10 GF_095Siren_DrawModel, 156 insns
	uint32_t __cdecl a_747400(uint32_t a1); // 0x747400 sub_747400, 15 insns
	uint32_t __cdecl a_747440(uint32_t a1); // 0x747440 MAG_095_sub_747440, 40 insns
	uint32_t __cdecl a_7474B0(uint32_t a1); // 0x7474B0 MAG_095_sub_7474B0, 9 insns
	uint32_t __cdecl a_7474D0(uint32_t a1); // 0x7474D0 MAG_095_sub_7474D0, 8 insns
	uint32_t __cdecl a_747500(uint32_t a1); // 0x747500 MAG_095_sub_747500, 19 insns
	uint32_t __cdecl a_747550(uint32_t a1); // 0x747550 MAG_095_sub_747550, 5 insns
	uint32_t __cdecl a_747560(uint32_t a1); // 0x747560 MAG_095_sub_747560, 13 insns
	uint32_t __cdecl a_747590(uint32_t a1); // 0x747590 MAG_095_sub_747590, 6 insns
	uint32_t __cdecl a_7475A0(uint32_t a1); // 0x7475A0 MAG_095_sub_7475A0, 6 insns
	uint32_t __cdecl a_732120(uint32_t a1); // 0x732120 MAG_096_sub_732120, 15 insns
	uint32_t __cdecl a_732160(void); // 0x732160 MAG_096_sub_732160, 15 insns
	uint32_t __cdecl a_7322A0(void); // 0x7322A0 MAG_096_sub_7322A0, 40 insns
	uint32_t __cdecl a_732A00(uint32_t a1); // 0x732A00 sub_732A00 TASK, 41 insns
	uint32_t __cdecl a_7334C0(uint32_t a1); // 0x7334C0 sub_7334C0, 13 insns
	uint32_t __cdecl a_7335D0(uint32_t a1); // 0x7335D0 sub_7335D0, 18 insns
	uint32_t __cdecl a_733760(uint32_t a1); // 0x733760 sub_733760, 8 insns
	uint32_t __cdecl a_733950(uint32_t a1, uint32_t a2, uint32_t a3); // 0x733950 sub_733950, 83 insns
	uint32_t __cdecl a_733AC0(uint32_t a1); // 0x733AC0 sub_733AC0, 6 insns
	uint32_t __cdecl a_733AE0(uint32_t a1); // 0x733AE0 sub_733AE0, 8 insns
	uint32_t __cdecl a_733B70(uint32_t a1); // 0x733B70 sub_733B70, 8 insns
	uint32_t __cdecl a_7340A0(uint32_t a1, uint32_t a2); // 0x7340A0 sub_7340A0, 247 insns
	uint32_t __cdecl a_7348A0(uint32_t a1); // 0x7348A0 sub_7348A0, 15 insns
	uint32_t __cdecl a_7353B0(uint32_t a1); // 0x7353B0 sub_7353B0, 39 insns
	uint32_t __cdecl a_735440(void); // 0x735440 sub_735440, 28 insns
	uint32_t __cdecl a_7354A0(uint32_t a1, uint32_t a2); // 0x7354A0 sub_7354A0, 94 insns
	uint32_t __cdecl a_7355E0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x7355E0 sub_7355E0, 135 insns
	uint32_t __cdecl a_735BB0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x735BB0 sub_735BB0, 194 insns
	uint32_t __cdecl a_735DF0(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4); // 0x735DF0 sub_735DF0, 224 insns
	uint32_t __cdecl a_736C00(void); // 0x736C00 sub_736C00, 21 insns
	uint32_t __cdecl a_737ED0(uint32_t a1); // 0x737ED0 sub_737ED0, 27 insns
	uint32_t __cdecl a_737F50(uint32_t a1); // 0x737F50 sub_737F50, 50 insns
	uint32_t __cdecl a_737FD0(uint32_t a1, uint32_t a2, uint32_t a3); // 0x737FD0 ?, 509 insns
	uint32_t __cdecl a_7387B0(uint32_t a1, uint32_t a2); // 0x7387B0 sub_7387B0, 59 insns
	uint32_t __cdecl a_742FF0(uint32_t a1); // 0x742FF0 sub_742FF0, 76 insns
	uint32_t __cdecl a_7395E0(uint32_t a1); // 0x7395E0 sub_7395E0, 13 insns
	uint32_t __cdecl a_739890(uint32_t a1); // 0x739890 sub_739890, 46 insns
	uint32_t __cdecl a_739960(uint32_t a1, uint32_t a2, uint32_t a3); // 0x739960 sub_739960, 74 insns
	uint32_t __cdecl a_739AC0(uint32_t a1, uint32_t a2, uint32_t a3); // 0x739AC0 sub_739AC0, 10 insns
	uint32_t __cdecl a_739B40(uint32_t a1); // 0x739B40 sub_739B40, 16 insns
	uint32_t __cdecl a_8DDC30(uint32_t a1); // 0x8DDC30 sub_8DDC30 TASK, 32 insns
	uint32_t __cdecl a_8DDCA0(uint32_t a1); // 0x8DDCA0 sub_8DDCA0, 12 insns
	uint32_t __cdecl a_8DDCD0(uint32_t a1); // 0x8DDCD0 sub_8DDCD0, 108 insns

	// ------------------------------------------------------------------------------------
	// module descriptor
	// ------------------------------------------------------------------------------------
	struct Mod
	{
		const char *name;
		int effect_id;
		uint32_t lo, hi;  // module code range
		uint32_t g[NG];   // module globals by canonical index (0 = the module has no such global)
		uint32_t f[NF];   // module functions by canonical index (original addresses)
	};
	extern const Mod *g_mod;   // module of the effect running now
	extern const Mod MOD_090, MOD_095, MOD_096, MOD_097, MOD_098, MOD_099, MOD_100;
	inline uint32_t G(int k) { return g_mod->g[k]; }
	inline uint32_t F(int k) { return g_mod->f[k]; }
	const Mod *find_mod(int effect_id);
	// selects the module whose code range holds code_addr (a master task passes its own node's
	// original function address, node +8) - every master port calls this first
	void set_mod_by_code(uint32_t code_addr);

	// ------------------------------------------------------------------------------------
	// dispatch
	// ------------------------------------------------------------------------------------
	void *port_of(uint32_t orig);                 // port of an original address, or nullptr
	void add_port(uint32_t orig, void *port);
	void remove_port(uint32_t orig);               // (harness bisection)
	template<typename... A> inline uint32_t callo(uint32_t addr, A... a) { return ((uint32_t (__cdecl *)(A...))addr)(a...); }
	template<typename... A> inline uint32_t callp(uint32_t addr, A... a)
	{
		void *p = port_of(addr);
		return ((uint32_t (__cdecl *)(A...))(p ? p : (void *)addr))(a...);
	}

	// registers the engine ports of a module (all its addresses served by an engine port) in the
	// dispatch table and in the task registry (register_port)
	// held_tasks: 0-terminated list of this module's original task addresses whose drawing is
	// redrawn by the module's held frame (registered held = true)
	void register_module(int effect_id, const uint32_t *held_tasks = nullptr);
	// the module's own ports (s_/m_ functions) go through this, so they are dispatchable too
	void register_module_port(int effect_id, uint32_t orig, void *port, const char *name, bool held = false);

	// ------------------------------------------------------------------------------------
	// modes
	// ------------------------------------------------------------------------------------
	// real tick on which a ported master last ran (held frames need their memos)
	extern uint32_t g_ported_tick;
	// camera steppers a_73AE10 (kind 1) / a_73B4B0 (kind 2) note the real tick they ran on
	extern uint32_t g_cam_tick;
	extern int g_cam_kind;
	// held-frame camera of the engine's camera script (register_module_camera)
	bool held_camera(int num, int den, int16_t world[3], int16_t lookat[3]);
	// held frame (30 fps) of a module: in-between redraw of the prim-model plays of this real tick
	// and of the creature actor(s) (task function creature_task in creature_queue, drawn with
	// a_746C10(node, draw_arg, cursor)); module globals [bss_lo, bss_hi) are saved/restored
	struct HeldDesc { const Mod *mod; uint32_t creature_queue, creature_task, draw_arg, bss_lo, bss_hi; };
	void held_frame(const HeldDesc &d, int num, int den);
	bool held_ready();
	// prim-model player twin (replaces calls of MAG_011_sub_701970(layout, cb, arg, paused)): runs
	// ff8fx::prim::play with the port of cb and remembers the play for the held frame (arg = the
	// 0x5C-byte parameter block of the family's prim-model draw)
	uint32_t prim_play(uint32_t layout, uint32_t cb, uint32_t arg, uint32_t paused);
	// held frame: in-between redraw of every prim play of the current module on this real tick
	void held_draw_prim_plays(int num, int den);
}
}
