/****************************************************************************/
//    Copyright (C) 2026 Julian Xhokaxhiu                                   //
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

// Engine of the cinematic GFs

#pragma once

#include "fx_port.h"

namespace ff8fx
{
namespace gfc
{
	using namespace ff8fx::eng;

	template<typename T> inline T &MEM(uint32_t addr) { return *(T *)addr; }
	template<typename T> inline T &AT(const void *p, int32_t off) { return *(T *)((uint8_t *)p + off); }
	inline int8_t &S8(const void *p, int32_t o) { return AT<int8_t>(p, o); }
	inline uint8_t &U8(const void *p, int32_t o) { return AT<uint8_t>(p, o); }
	inline int16_t &S16(const void *p, int32_t o) { return AT<int16_t>(p, o); }
	inline uint16_t &U16(const void *p, int32_t o) { return AT<uint16_t>(p, o); }
	inline int32_t &S32(const void *p, int32_t o) { return AT<int32_t>(p, o); }
	inline uint32_t &U32(const void *p, int32_t o) { return AT<uint32_t>(p, o); }
	inline uint8_t *&PTR(const void *p, int32_t o) { return AT<uint8_t *>(p, o); }
	inline int32_t add32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
	inline int32_t sub32(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }

	typedef void (__cdecl *VoidFn)();

	inline uint8_t *&RT() { return MEM<uint8_t *>(us(0x27973B8)); }
	inline uint8_t *&RCTX() { return MEM<uint8_t *>(us(0x27973BC)); }
	inline uint8_t *&SEQ() { return MEM<uint8_t *>(us(0x27973C0)); }
	inline uint8_t *&PTR3() { return MEM<uint8_t *>(us(0x27973C4)); }
	inline uint8_t *&CUR() { return MEM<uint8_t *>(us(0x27973E8)); }
	inline uint8_t *&CTX() { return MEM<uint8_t *>(us(0x27973EC)); }
	inline uint8_t *&SCENE() { return MEM<uint8_t *>(us(0x279744C)); }
	inline uint8_t *&STREAM() { return MEM<uint8_t *>(us(0x2797450)); }
	inline uint8_t *&WS() { return MEM<uint8_t *>(us(0x2797624)); }
	inline uint8_t *ORDER() { return (uint8_t *)us(0x2797454); }
	inline uint8_t *DRAWORDER() { return (uint8_t *)us(0x2797554); }
	inline uint16_t *NODEKEYS() { return (uint16_t *)us(0x2797204); }
	inline Mat4x3 *NODEMAT() { return (Mat4x3 *)us(0x27979E8); }
	inline Mat4x3 *CAMALT() { return (Mat4x3 *)us(0x2796F90); }
	inline uint8_t *LIGHTS() { return (uint8_t *)us(0x27977A4); }
	inline Mat4x3 *BBMAT() { return (Mat4x3 *)us(0x2797968); }
	inline uint8_t **RESFILE() { return (uint8_t **)us(0x2798A68); }
	inline uint32_t *DEPTHARR() { return (uint32_t *)us(0x2798C18); }
	inline float &FLT_1877DA8() { return MEM<float>(us(0x1877DA8)); }
	inline uint32_t &PKTCUR() { return MEM<uint32_t>(us(0x1D8E054)); }
	inline int16_t &PROJH() { return MEM<int16_t>(us(0x1D8E038)); }
	inline uint32_t &FRAMERL() { return MEM<uint32_t>(us(0x1D8E04C)); }
	inline uint32_t &UPDFLAGS() { return MEM<uint32_t>(us(0x1D96A9C)); }
	inline Mat4x3 &BATTLEVIEW() { return MEM<Mat4x3>(us(0x1D97778)); }
	inline int16_t *CAMEYE() { return (int16_t *)us(0xB8B7F0); }
	inline int16_t *CAMAT() { return (int16_t *)us(0xB8B7F8); }

	static const uint32_t STATE_LO = 0x2796E00, STATE_HI = 0x2798C18;

	struct Clone
	{
		const char *name;
		int effect_id;
		uint32_t vm_table;
		uint32_t bone_table;
		uint32_t prim_table;
		uint32_t prim_size;
		uint32_t spr_table;
		uint32_t ptype_table;
		uint32_t pop_table;
		uint32_t draw_table;
		uint32_t attr_16A;
		uint32_t attr_184;
		uint32_t attr_186;
		uint32_t attr_194;
		uint32_t attr_19C;
		uint32_t desc;
		uint32_t ptr_block;
		uint32_t queue;
		uint32_t file00, file01;
		uint32_t tick;
		uint32_t load_cb;
		bool lit_dispatcher;
		int bone_count;
		VoidFn vm[0x200];
		VoidFn bigtab[256];
		VoidFn *bone, *prim, *spr, *ptype, *pop;
		VoidFn draw[256];
	};
	extern Clone *g_clone;
	inline Clone &C() { return *g_clone; }

	struct Generic
	{
		VoidFn vm[0x200];
		VoidFn bone[32];
		VoidFn prim[60];
		VoidFn spr[11];
		VoidFn ptype[5];
		VoidFn pop[24];
		VoidFn draw[73];
	};
	extern Generic g_generic;
	void fill_generic();

	struct Exception { int table; int index; };
	void init_clone(Clone &c, const Exception *ex, int nex);
	Clone *find_clone(int effect_id);
}
}

namespace ff8fx
{
namespace gfc
{
#define GFC_TAINT(d, s, n) do { } while (0)
	inline void call_vm(uint32_t op) { C().vm[op & 0x1FF](); }

	uint32_t SequenceTick(Clone &c);
	void BindContext();
	void ReleaseContext();
	void AnimChannelsNeg();
	void AnimChannelsPos();
	void AnimIntegrator();
	void BuildMatricesAndDraw();

	namespace x
	{
		template<typename F> inline F f(uint32_t a) { return (F)a; }
		inline void GteWriteData(int32_t value, int reg) { f<void (__cdecl *)(int32_t, int)>(us(0x45D7C0))(value, reg); }
		inline void GteReadData(void *dst, int reg) { f<void (__cdecl *)(void *, int)>(us(0x45D7A0))(dst, reg); }
		inline void GteReadCtrl(void *dst, int reg) { f<void (__cdecl *)(void *, int)>(us(0x45D7D0))(dst, reg); }
		inline void GteWriteCtrl(int32_t value, int reg) { f<void (__cdecl *)(int32_t, int)>(us(0x45D7F0))(value, reg); }
		inline void GteWriteData2(int reg, int32_t value) { f<void (__cdecl *)(int, int32_t)>(us(0x45DB90))(reg, value); }
		inline void GteReadData2(int reg, void *dst) { f<void (__cdecl *)(int, void *)>(us(0x45DBA0))(reg, dst); }
		inline void GteSetBackColor3(int32_t r, int32_t g, int32_t b) { f<void (__cdecl *)(int32_t, int32_t, int32_t)>(us(0x45DDA0))(r, g, b); }
		inline void GteSetRotMatrix(const void *m) { f<void (__cdecl *)(const void *)>(us(0x45DE00))(m); }
		inline void GteSetTransVector(const void *m) { f<void (__cdecl *)(const void *)>(us(0x45DEF0))(m); }
		inline void GteLoadV0(const void *v) { f<void (__cdecl *)(const void *)>(us(0x45DF80))(v); }
		inline void GteLoadV012(const void *v0, const void *v1, const void *v2) { f<void (__cdecl *)(const void *, const void *, const void *)>(us(0x45DFE0))(v0, v1, v2); }
		inline void GteReadOTZdiv4(void *dst) { f<void (__cdecl *)(void *)>(us(0x45E250))(dst); }
		inline void GteReadSXY2(void *dst) { f<void (__cdecl *)(void *)>(us(0x45E260))(dst); }
		inline void GteReadSXY012(void *a, void *b, void *c) { f<void (__cdecl *)(void *, void *, void *)>(us(0x45E270))(a, b, c); }
		inline void GteReadOTZ(void *dst) { f<void (__cdecl *)(void *)>(us(0x45E3D0))(dst); }
		inline void Gte_45E580() { f<void (__cdecl *)()>(us(0x45E580))(); }
		inline void GteAVSZ3() { f<void (__cdecl *)()>(us(0x45E5C0))(); }
		inline void GteNCLIP() { f<void (__cdecl *)()>(us(0x45EE10))(); }
		inline void GteRTPS() { f<void (__cdecl *)()>(us(0x45FAA0))(); }
		inline void GteRTPT() { f<void (__cdecl *)()>(us(0x45FE10))(); }
		inline void Gte_4601B0() { f<void (__cdecl *)()>(us(0x4601B0))(); }
		inline void GteMVMVA_RotV0Tr() { f<void (__cdecl *)()>(us(0x460840))(); }
		inline void SetRotMatrix(const void *m) { f<void (__cdecl *)(const void *)>(us(0x56BAE0))(m); }
		inline void SetLightMatrix(const void *m) { f<void (__cdecl *)(const void *)>(us(0x56BB00))(m); }
		inline void SetColorMatrix(const void *m) { f<void (__cdecl *)(const void *)>(us(0x56BB20))(m); }
		inline void SetTransVector(const void *m) { f<void (__cdecl *)(const void *)>(us(0x56BB30))(m); }
		inline void ScaleMatrix(void *m, const void *v3) { f<void (__cdecl *)(void *, const void *)>(us(0x56BEF0))(m, v3); }
		inline void *MulMatrix3(const void *a, const void *b, void *out) { return f<void *(__cdecl *)(const void *, const void *, void *)>(us(0x56C090))(a, b, out); }
		inline void *MulMatrixInPlace(void *m, const void *n) { return f<void *(__cdecl *)(void *, const void *)>(us(0x56C220))(m, n); }
		inline void *MulMatrixGte(const void *a, void *b) { return f<void *(__cdecl *)(const void *, void *)>(us(0x56C270))(a, b); }
		inline void Matrix_56C2C0(const void *m) { f<void (__cdecl *)(const void *)>(us(0x56C2C0))(m); }
		inline void *ComposeAffine(const void *a, const void *b, void *out) { return f<void *(__cdecl *)(const void *, const void *, void *)>(us(0x56C2F0))(a, b, out); }
		inline void TransformToCamera(const void *v, void *mac, void *flag) { f<void (__cdecl *)(const void *, void *, void *)>(us(0x56C820))(v, mac, flag); }
		inline void SetBackColor(int32_t r, int32_t g, int32_t b) { f<void (__cdecl *)(int32_t, int32_t, int32_t)>(us(0x56CC30))(r, g, b); }
		inline void Fog_56CCA0(int32_t a, int32_t b) { f<void (__cdecl *)(int32_t, int32_t)>(us(0x56CCA0))(a, b); }
		inline void Fog_56CCC0(int32_t a, int32_t b) { f<void (__cdecl *)(int32_t, int32_t)>(us(0x56CCC0))(a, b); }
		inline void ParseCamera2(int32_t h) { f<void (__cdecl *)(int32_t)>(us(0x56CD00))(h); }
		inline void *RotMatrixFromAngles(const void *angles, void *out) { return f<void *(__cdecl *)(const void *, void *)>(us(0x56CD50))(angles, out); }
		inline int32_t Cos(int32_t a) { return f<int32_t (__cdecl *)(int32_t)>(us(0x56D100))(a); }
		inline int32_t Sin(int32_t a) { return f<int32_t (__cdecl *)(int32_t)>(us(0x56D130))(a); }
		inline void SetDrawMode(void *p, int32_t dfe, int32_t dtd, int32_t tpage, const void *rect) { f<void (__cdecl *)(void *, int32_t, int32_t, int32_t, const void *)>(us(0x45BFC0))(p, dfe, dtd, tpage, rect); }
		inline void InsertPrimDepthKeys(uint32_t ot, void *prim, int32_t a, int32_t b, int32_t c, int32_t d) { f<void (__cdecl *)(uint32_t, void *, int32_t, int32_t, int32_t, int32_t)>(us(0x45C870))(ot, prim, a, b, c, d); }
		inline void InsertPrimAltViewport(uint32_t ot, void *prim) { f<void (__cdecl *)(uint32_t, void *)>(us(0x45C8E0))(ot, prim); }
		inline void ClearOT(uint32_t ot, int32_t n) { f<void (__cdecl *)(uint32_t, int32_t)>(us(0x45D530))(ot, n); }
		inline void BuildBoneMatricesFromPose(void *hdr) { f<void (__cdecl *)(void *)>(us(0x508C90))(hdr); }
		inline int32_t ReadAnimation(void *hdr, void *cmd) { return f<int32_t (__cdecl *)(void *, void *)>(us(0x508F90))(hdr, cmd); }
		inline int32_t PreReadAnimation(void *hdr, void *cmd, int32_t anim) { return f<int32_t (__cdecl *)(void *, void *, int32_t)>(us(0x509440))(hdr, cmd, anim); }
		inline void ComputeBonesWorldMatrices(void *hdr, void *root) { f<void (__cdecl *)(void *, void *)>(us(0x5095B0))(hdr, root); }
		inline uint32_t RenderGeometry(void *a, void *b, uint32_t ot, int32_t mode, uint32_t cursor) { return f<uint32_t (__cdecl *)(void *, void *, uint32_t, int32_t, uint32_t)>(us(0x5099D0))(a, b, ot, mode, cursor); }
		inline uint32_t Render_5088A0(void *a, uint32_t ot, int32_t mode, uint32_t cursor) { return f<uint32_t (__cdecl *)(void *, uint32_t, int32_t, uint32_t)>(us(0x5088A0))(a, ot, mode, cursor); }
		inline int32_t Rand() { return f<int32_t (__cdecl *)()>(us(0x55CBD2))(); }
		inline void *FieldAlloc(int32_t n) { return f<void *(__cdecl *)(int32_t)>(us(0x5082B0))(n); }
		inline void FieldFree(int32_t n) { f<void (__cdecl *)(int32_t)>(us(0x5082D0))(n); }

		inline void PlaySE(void *snd, int32_t a, int32_t b) { f<void (__cdecl *)(void *, int32_t, int32_t)>(us(0x501330))(snd, a, b); }
		inline void TransSummonStream(void *a, void *b) { f<void (__cdecl *)(void *, void *)>(us(0x501860))(a, b); }
		inline void PlaySummonStream(int32_t a, int32_t b, int32_t c) { f<void (__cdecl *)(int32_t, int32_t, int32_t)>(us(0x5018C0))(a, b, c); }
		inline void Snd_46B3A0(int32_t a) { f<void (__cdecl *)(int32_t)>(us(0x46B3A0))(a); }
		inline void Snd_46B3E0(int32_t a) { f<void (__cdecl *)(int32_t)>(us(0x46B3E0))(a); }
		inline void Snd_46B450(int32_t a, int32_t b) { f<void (__cdecl *)(int32_t, int32_t)>(us(0x46B450))(a, b); }
		inline void MusicSetVolumeTrans(int32_t a, int32_t b, int32_t c) { f<void (__cdecl *)(int32_t, int32_t, int32_t)>(us(0x46BBC0))(a, b, c); }
		inline void Music_47E3C0(int32_t a) { f<void (__cdecl *)(int32_t)>(us(0x47E3C0))(a); }
		inline void PreLoadBattleFile(int32_t id, void *dst, int32_t a, uint32_t cb) { f<void (__cdecl *)(int32_t, void *, int32_t, uint32_t)>(us(0x48D0A0))(id, dst, a, cb); }
		inline void Snd_4A2940(int32_t a) { f<void (__cdecl *)(int32_t)>(us(0x4A2940))(a); }
		inline uint32_t ClaimVoiceSlot(const void *snd, int32_t a, int32_t b) { return f<uint32_t (__cdecl *)(const void *, int32_t, int32_t)>(us(0x4A29A0))(snd, a, b); }
		inline void Battle_4A8480(int32_t a) { f<void (__cdecl *)(int32_t)>(us(0x4A8480))(a); }
		inline void Battle_501E40(int32_t a) { f<void (__cdecl *)(int32_t)>(us(0x501E40))(a); }
		inline void Battle_501F30(int32_t a) { f<void (__cdecl *)(int32_t)>(us(0x501F30))(a); }
		inline void Camera_504270(int32_t a) { f<void (__cdecl *)(int32_t)>(us(0x504270))(a); }
		inline void QueueVramUpload(const void *rect, const void *data) { f<void (__cdecl *)(const void *, const void *)>(us(0x505DF0))(rect, data); }
		inline void ApplyActionResultToTarget(int32_t a) { f<void (__cdecl *)(int32_t)>(us(0x506690))(a); }
		inline void ApplyActionResultToTargets(int32_t a, int32_t b) { f<void (__cdecl *)(int32_t, int32_t)>(us(0x506BA0))(a, b); }
		inline void Battle_508630(void *a, void *b) { f<void (__cdecl *)(void *, void *)>(us(0x508630))(a, b); }
		inline void PlayCameraAnimation(void *a) { f<void (__cdecl *)(void *)>(us(0x5099A0))(a); }
		inline void CameraArmReturn() { f<void (__cdecl *)()>(us(0x50A730))(); }
	}

	inline void pad_after_rot_mul(void *m, const void *angles) { U16(m, 0x12) = S16(angles, 2) < 0 ? 0xFFFF : 0; }

	namespace blob
	{
		template<typename F> inline F f(uint32_t a) { return (F)a; }
		inline void InitBone(int32_t id) { f<int32_t (__cdecl *)(int32_t)>(us(0xB65160))(id); }
		inline void OrderListRemove(int32_t id) { f<int32_t (__cdecl *)(int32_t)>(us(0xB651E0))(id); }
		inline void DrawListRemove(int32_t id) { f<int32_t (__cdecl *)(int32_t)>(us(0xB65270))(id); }
		inline void DrawListAddId(int32_t id) { f<int32_t (__cdecl *)(int32_t)>(us(0xB652E0))(id); }
		inline void SetDrawHandler(int32_t h) { f<int32_t (__cdecl *)(int32_t)>(us(0xB65320))(h); }
		inline uint8_t *GetBone(int32_t ref) { return f<uint8_t *(__cdecl *)(int32_t)>(us(0xB65370))(ref); }
		inline Mat4x3 *NodeForCurBone() { return f<Mat4x3 *(__cdecl *)()>(us(0xB65480))(); }
		inline Mat4x3 *GetParentMatrix(int32_t ref) { return f<Mat4x3 *(__cdecl *)(int32_t)>(us(0xB65520))(ref); }
		inline Mat4x3 *RotMatrixOrder(const void *angles, int32_t order) { return f<Mat4x3 *(__cdecl *)(const void *, int32_t)>(us(0xB65590))(angles, order); }
		inline void CopyMatrix(void *dst, const void *src) { f<void (__cdecl *)(void *, const void *)>(us(0xB656A0))(dst, src); }
		inline uint8_t *ArenaAlloc(int32_t n)
		{
			return f<uint8_t *(__cdecl *)(int32_t)>(us(0xB656E0))(n);
		}
		inline void ArenaFree_B65710() { f<int32_t (__cdecl *)()>(us(0xB65710))(); }
		inline int32_t Rand73(int32_t range) { return f<int32_t (__cdecl *)(int32_t)>(us(0xB65740))(range); }
		inline int32_t Rand74(int32_t range) { return f<int32_t (__cdecl *)(int32_t)>(us(0xB657A0))(range); }
		inline uint8_t *ObjectPtr(int32_t id) { return f<uint8_t *(__cdecl *)(int32_t)>(us(0xB657E0))(id); }
		inline uint8_t *ObjectPtrWs(int32_t id) { return f<uint8_t *(__cdecl *)(int32_t)>(us(0xB66230))(id); }
		inline void SetSpriteAnim(int32_t id) { f<int32_t (__cdecl *)(int32_t)>(us(0xB66270))(id); }
		inline uint8_t *VramRectRing() { return f<uint8_t *(__cdecl *)()>(us(0xB663A0))(); }
		inline int32_t AltTextureUpload(const void *rect, const void *data)
		{
			return f<int32_t (__cdecl *)(const void *, const void *)>(us(0xB663C0))(rect, data);
		}
		inline void ReadAltTexture(int32_t id) { f<int32_t (__cdecl *)(int32_t)>(us(0xB664A0))(id); }
		inline void ReadClut_B66560(int32_t id) { f<int32_t (__cdecl *)(int32_t)>(us(0xB66560))(id); }
		inline void TexInfo_B665C0(int32_t id) { f<int32_t (__cdecl *)(int32_t)>(us(0xB665C0))(id); }
		inline int32_t ClutWord_B66640(int32_t id) { return f<int32_t (__cdecl *)(int32_t)>(us(0xB66640))(id); }
		inline void ClutTint_B666F0(int32_t r, int32_t g, int32_t b) { f<int32_t (__cdecl *)(int32_t, int32_t, int32_t)>(us(0xB666F0))(r, g, b); }
		inline void Blob_B66A90() { f<int32_t (__cdecl *)()>(us(0xB66A90))(); }
		inline int32_t Blob_B66B30(int32_t a, int32_t b, int32_t c, int32_t d) { return f<int32_t (__cdecl *)(int32_t, int32_t, int32_t, int32_t)>(us(0xB66B30))(a, b, c, d); }
		inline void Blob_B66B80(void *a, int32_t b, int32_t c, void *d) { f<int32_t (__cdecl *)(void *, int32_t, int32_t, void *)>(us(0xB66B80))(a, b, c, d); }
		inline void LookAt_B66E50(const void *a, const void *b, void *out) { f<int32_t (__cdecl *)(const void *, const void *, void *)>(us(0xB66E50))(a, b, out); }
	}

	void h_B269E0();
	int32_t h_B26AD0(uint8_t *blk);
	uint8_t *h_B26860(int32_t id);
	void h_B2C440();
	int32_t h_B2D460();
	int32_t h_B2F8F0();
	int32_t h_B2FC20();
	void h_B27000();
	void h_B27130();
	void h_B27360(const void *pos, const void *angles, int32_t scale, int32_t order);
	void h_B27440(const int16_t *clipArg);
	void h_B29450(int32_t c);

	void fill_core(Generic &g);
	void fill_vm_a(Generic &g);
	void fill_vm_b(Generic &g);
	void fill_vm_c(Generic &g);
	void fill_vm_d(Generic &g);
	void fill_draw_mesh(Generic &g);
	void fill_draw_prim(Generic &g);
	void fill_draw_sprite(Generic &g);

	void apply_shared_ribbon(Clone &c);
	void apply_shared_misc(Clone &c);
}
}
