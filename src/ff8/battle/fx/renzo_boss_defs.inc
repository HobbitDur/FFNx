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

// Squall's boss-specific Renzokuken modules 334 / 335 / 336 / 337 / 339 / 340 / 341 / 342 / 343 (the TARGET's info +0x150
// table picks them, computeCommandAction 0x48DE0B): their common part, first half (node / pool / record layouts, engine
// calls, the weapon bone and the module's accessors). Included INSIDE the module's namespace by the module file, after
// it declares its addresses:
//   module globals: CONTEXT, CASTER_SLOT, TARGET_SLOT, TEX_FILE, LENGTH, FINISHER, TEX_BASE, PACKET_CURSOR, Q_MASTER,
//     MASTER_POOL, Q_MAIN, MAIN_POOL, POOL_A / POOL_A_END (flashes), POOL_B / POOL_B_SPAWN_END / POOL_B_END (sparks), POOL_C /
//     POOL_C_END (debris), HIT_REC, PTS_A / PTS_B, SPL_A / SPL_B, HIT_POS, HIT_DIR, PREV_HIT (the hit point / direction of
//     the trail's previous tick, 0x18 bytes), RING / RING_END (trail ring), SLOTS / SLOTS_END (100 x 0x20 records), HIT_OFFS;
//   data: SOUND, TEXTURE_NAME, CAMERA, ANIM_SETS, BIND_OUT, MODEL_SPARKLE, SPARKLE_FADE, SPARKLE_DEPTH, MODEL_SLASH,
//     MODEL_STREAK, SEQ_FLASH, SEQ_DEBRIS, SEQ_SPARK, SPARK_FRAMES, SLASH_REF, HIT_LEAD;
//   tasks: ORIG_FileLoader, ORIG_Setup, ORIG_Controller, ORIG_HitSpark, ORIG_Sparkle, ORIG_Trail, ORIG_SparkSpawn, ORIG_Hit,
//     ORIG_Slash, ORIG_Master;
//   PARITY (the packet arena's odd-tick offset).
// After it the module defines GAME, its RenzoBossParams (below): its own data, the defaults of its parameters.
// The second half (the common tasks) is renzo_boss.inc, its 30 fps part renzo_boss_held.inc.
//
// Fujin (FF8UltimateEditor's effect tool) runs these modules from their descriptions: every module address goes through
// own::R and every task the code starts through own::Fn (a program runs them on a memory block of its own), the data
// through the parameters (fx_params.h: the game's data unless edited).
//
// The common code is effect 332's (mag332_renzokuken_7hits.cpp: hit points through the caster's matrix + the module's
// offset table, which is all zero in these modules: a zero-initialised table in the module's bss) with 161's second
// animation set for the boss (BindBoth) and two additions: a table of 100 records of 0x20 bytes (SLOTS, freed (0) by the
// setup; AddSlotTask gives a task one, marked -1, its index in the node +0xE) for the module's own particles, and the trail
// keeps the hit point / direction of its previous tick (PREV_HIT) before it computes the new ones.

	using namespace eng;
	using namespace magc;

	// --- module globals (own::R: where the running copy has them) ---
	inline int32_t &CasterSlot() { return var<int32_t>(own::R(CASTER_SLOT)); }
	inline int32_t &TargetSlot() { return var<int32_t>(own::R(TARGET_SLOT)); }  // the boss (first target)
	inline int32_t &Length() { return var<int32_t>(own::R(LENGTH)); }          // sum of the stage durations (4 until tick 2)
	inline int32_t &Finisher() { return var<int32_t>(own::R(FINISHER)); }      // a finisher follows (action 0 word +4 == 0xFFFE)
	inline uint32_t &TexBase() { return var<uint32_t>(own::R(TEX_BASE)); }     // Magic_TextureOFF
	inline uint32_t &PacketCursor() { return var<uint32_t>(own::R(PACKET_CURSOR)); }
	inline uint32_t &TexFile() { return var<uint32_t>(own::R(TEX_FILE)); }     // the effect's file (its TIM)
	inline TaskQueue *QMaster() { return (TaskQueue *)own::R(Q_MASTER); }
	inline TaskQueue *QMain() { return (TaskQueue *)own::R(Q_MAIN); }
	inline TaskQueue *QHit() { return (TaskQueue *)0x209FAA8; }      // the battle's hit-effect queue
	// stage durations of the caster's sequence (AnimSeq opcode AB: 0x1D98214 = n, n + 1 frame counts from 0x1D98215)
	inline uint8_t StageCount() { return var<uint8_t>(0x1D98214); }
	inline uint8_t StageLen(int i) { return var<uint8_t>(0x1D98215 + i); }

	// engine functions not in fx_port.h / mag_common.h
	inline int32_t Normalize(const int32_t *in, int32_t *out) { return fn<int32_t (__cdecl *)(const int32_t *, int32_t *)>(0x56BC50)(in, out); } // NormalizeVectorToFixedPoint
	inline void NormalizeS16(int16_t *in, int16_t *out) { fn<void (__cdecl *)(int16_t *, int16_t *)>(0x56BDE0)(in, out); }        // VectorNormalS16
	inline int32_t SqrtInt(int32_t v) { return fn<int32_t (__cdecl *)(int32_t)>(0x56BEC0)(v); }                                       // Math_SqrtInt
	inline void GetEffectSpawnPosition(void *entity, int32_t bone, int32_t frac, int16_t *out) { fn<void (__cdecl *)(void *, int32_t, int32_t, int16_t *)>(0x502170)(entity, bone, frac, out); }
	inline void MatrixMultiply(const Mat4x3 *a, Mat4x3 *b) { fn<void (__cdecl *)(const Mat4x3 *, Mat4x3 *)>(0x56C270)(a, b); }     // GTE_MatrixMultiply
	inline void MatVec(const Mat4x3 *m, const int16_t *in, int16_t *out) { fn<void (__cdecl *)(const Mat4x3 *, const int16_t *, int16_t *)>(0x56C4F0)(m, in, out); } // matrixMultiplyVector
	inline void SplineSetup(int32_t n, const void *points, void *work) { fn<void (__cdecl *)(int32_t, const void *, void *)>(0x571620)(n, points, work); }       // Effect_SplineSetup
	inline void SplineEval(int32_t n, void *work, void *out, int32_t t) { fn<void (__cdecl *)(int32_t, void *, void *, int32_t)>(0x571690)(n, work, out, t); } // Effect_SplineEval
	inline void GteSetFarColor(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DD60)(r, g, b); }   // GTE_SetFarColor
	inline void GteSetFarColor2(int32_t r, int32_t g, int32_t b) { fn<void (__cdecl *)(int32_t, int32_t, int32_t)>(0x45DDA0)(r, g, b); } // pre_someCameraWork_45DD60 (calls 0x45DD60)
	inline void GteLoadRGBC(const void *c) { fn<void (__cdecl *)(const void *)>(0x45E110)(c); }
	inline void GteDPCS() { fn<void (__cdecl *)()>(0x45F270)(); }
	inline void GteStoreRGB2(void *dst) { fn<void (__cdecl *)(void *)>(0x45E360)(dst); }
	inline void GteLoadV012(const void *v0, const void *v1, const void *v2) { fn<void (__cdecl *)(const void *, const void *, const void *)>(0x45DFE0)(v0, v1, v2); }
	inline void GteRTPT() { fn<void (__cdecl *)()>(0x45FE10)(); }
	inline void GteReadFLAG(void *dst) { fn<void (__cdecl *)(void *)>(0x45E210)(dst); }
	inline void GteReadSXY012Split(void *a, void *b, void *c) { fn<void (__cdecl *)(void *, void *, void *)>(0x45E270)(a, b, c); }
	inline void GteAVSZ4() { fn<void (__cdecl *)()>(0x45E610)(); }
	inline void GteReadOTZ32(void *dst) { fn<void (__cdecl *)(void *)>(0x45E3D0)(dst); }   // GTE_ReadOTZ
	inline void GteMVMVA_RotV0() { fn<void (__cdecl *)()>(0x460850)(); }                    // IR = MAC = R * V0
	inline void GteSetLightMatrix(const Mat4x3 *m) { fn<void (__cdecl *)(const Mat4x3 *)>(0x45DE50)(m); }        // GTE_SetLightMatrix
	inline void GteSetBackColorFromTrans(const Mat4x3 *m) { fn<void (__cdecl *)(const Mat4x3 *)>(0x64DDA0)(m); } // BK = the matrix's translation
	inline void GteMVMVA_LightV0_Bk() { fn<void (__cdecl *)()>(0x4607E0)(); }               // MAC = L * V0 + BK
	inline void GteSetRotScale(int32_t s) { fn<void (__cdecl *)(int32_t)>(0x64DE00)(s); }  // GTE_SetRotScale
	inline void GteSetTransFromVec32(const int32_t *v) { fn<void (__cdecl *)(const int32_t *)>(0x64DD70)(v); }
	inline uint32_t BindEntityModelFromComData(uint8_t *entity, uint32_t out, uint32_t data) { return fn<uint32_t (__cdecl *)(uint8_t *, uint32_t, uint32_t)>(0x5022C0)(entity, out, data); } // BS_BindEntityModelFromComData (returns the next out block)
	inline void QueueChainTransformation(uint8_t *entity, int32_t seq) { fn<void (__cdecl *)(uint8_t *, int32_t)>(0x505C00)(entity, seq); }
	inline void AddAnimSeqTask(uint8_t *entity, int32_t type) { fn<void (__cdecl *)(uint8_t *, int32_t)>(0x571840)(entity, type); }          // AddTaskToQueueAnimSeq_99_B0_B1_B4_A5

	// ------------------------------------------------------------------
	// Node layouts
	// ------------------------------------------------------------------
#pragma pack(push, 1)
	struct RootNode // master pool, 1 x 0x10
	{
		TaskNode hdr;
		int16_t counter;   // +0x0C
		int16_t pad0E;
	};
	struct Node24 // main pool and the hit-effect queue pool (0x209D078), 0x24 bytes
	{
		TaskNode hdr;
		int16_t c;         // +0x0C counter
		int16_t e;         // +0x0E controller: saved flag 0x800 / spawner: frame phase / slash: hit / sparkle: delay /
		                   //       slot tasks: their SLOTS record
		int16_t w10, w12, w14; // +0x10 position
		int16_t w16;       // +0x16 the position's 4th word
		int16_t w18;       // +0x18 angle / sparkle: bone
		int16_t w1A;       // +0x1A hit spark / sparkle: entity slot / emitters: the record kind they spawn
		int16_t w1C;       // +0x1C scale / sparkle: depth frame
		int16_t w1E;       // +0x1E scale rate
		int16_t w20;       // +0x20 second scale / sparkle: scale
		int16_t w22;       // +0x22 second scale rate
	};
	struct FlashP  // pool A
	{
		int16_t life, scale;
		int16_t pos[4];    // x, y, z, the 4th word of the hit point
	};
	struct SparkP  // pool B
	{
		int16_t life, scale;
		int16_t pos[3];
		int16_t vy;        // +0x0A
		int16_t frame;     // +0x0C 0..23
		int16_t pad0E;
	};
	struct DebrisP // pool C
	{
		int16_t life, scale;
		int16_t pos[4];    // x, y, z, the 4th word of the hit point
		int16_t vel[3];    // +0x0C
		int16_t pad12;
	};
	struct HitRec  // 0x9C bytes per hit
	{
		int16_t pos[4];    // +0x00 the hit point
		int32_t dir[4];    // +0x08 the direction (16 bytes)
		int32_t count;     // +0x18 streaks (16)
		int16_t angle[16]; // +0x1C streak angle about the direction
		int16_t width[16]; // +0x3C streak width (the length * 1.5 across, the length along)
		int16_t off[16];   // +0x5C streak offset along its axis
		int16_t speed[16]; // +0x7C added to the offset each tick, slowing
	};
	struct RingEntry   // trail ring, 0x14 bytes
	{
		int32_t alive;
		uint32_t base[2];  // x, y, z, pad
		uint32_t tip[2];
	};
	struct TrailWork   // the trail's Field_Alloc block (0x40 bytes)
	{
		uint8_t pad00[8];
		int32_t otz;       // +0x08
		uint32_t flag;     // +0x0C GTE FLAG
		uint32_t col0;     // +0x10 the previous quad's colour
		uint32_t col1;     // +0x14 the colour of this quad
		uint32_t pad18;
		uint32_t base_col; // +0x1C
		int16_t base[4];   // +0x20 the weapon point (4th word: the scratch's)
		int16_t tip[4];    // +0x28 666 along the blade (4th word: the scratch's)
		int32_t dir[4];    // +0x30 the blade direction (unit), 4th word unused
	};
	struct SlotRec     // SLOTS record, 0x20 bytes: a task's own (alive -1) or one of the module's particles (alive = its kind)
	{
		int32_t alive;     // +0x00 0 free, -1 a slot task's, else the kind (the emitter that spawned it draws it)
		int16_t life;      // +0x04
		int16_t scale;     // +0x06
		int16_t pos[4];    // +0x08 x, y, z, 4th word
		int16_t w10, w12, w14, w16, w18, w1A, w1C, w1E; // +0x10 per kind
	};
#pragma pack(pop)
	static_assert(sizeof(RootNode) == 0x10 && sizeof(Node24) == 0x24, "Renzokuken nodes");
	static_assert(sizeof(FlashP) == 0xC && sizeof(SparkP) == 0x10 && sizeof(DebrisP) == 0x14, "Renzokuken pools");
	static_assert(sizeof(HitRec) == 0x9C && sizeof(RingEntry) == 0x14 && sizeof(TrailWork) == 0x40, "Renzokuken records");
	static_assert(sizeof(SlotRec) == 0x20, "Renzokuken slot records");

	inline uint8_t *Caster() { return Entity(CasterSlot()); }
	inline uint8_t *Target() { return Entity(TargetSlot()); }
	inline SlotRec *Slot(int32_t i) { return (SlotRec *)(own::R(SLOTS) + 0x20 * i); }

	// ------------------------------------------------------------------
	// Parameters (Fujin's effect lab: fx_params.h): one layout for the nine modules; each module's GAME holds its own
	// data (the game's values: the game draws the same until one is edited)
	// ------------------------------------------------------------------
	struct RenzoBossParams
	{
		uint32_t sound;   // at the controller's tick 1
		struct { uint32_t flash, debris, spark, emit; } seqs; // the sprite sequences drawn (flipbooks; emit: the module's emitters', 0 none)
		struct { uint32_t sparkle, slash, streak, own[4]; } models; // the prim models drawn (meshes; own: the module's own, 0 none)
		struct { uint32_t texture_name, camera, anim_sets; } res; // its resources (data): texture file name, camera track, animation sets
		// its small tables (data): the hit leads, the hit offsets (0: the module's own zero table), the finisher sparkles'
		// fade by tick and depth word by frame
		struct { uint32_t hit_lead, hit_offs, sparkle_fade, sparkle_depth; } tables;
	};

	// ------------------------------------------------------------------
	// Weapon bone (the module's BoneFrame / WeaponBone)
	// ------------------------------------------------------------------
	// BoneFrame: bone `bone` of a model (0xF0 / 0xF1: the skeleton's effect bone selectors): its z axis (0, 0, -0x1000)
	// through the bone's rotation, normalized, and its translation (3 words)
	static void BoneFrame(uint8_t *anim_header, int32_t bone, int32_t *dir, int16_t *pos)
	{
		const uint8_t *sk = *(uint8_t **)*(uint8_t **)(anim_header + 4);
		if (bone >= 0xF0) bone = sk[bone - 0xEC];
		const uint8_t *b = sk + 0x10 + bone * 0x30;
		dir[1] = 0;
		dir[0] = 0;
		dir[2] = -0x1000;
		TransformVectorBy3x3Matrix((const Mat4x3 *)(b + 0x10), dir, dir);
		Normalize(dir, dir);
		pos[0] = *(const int16_t *)(b + 0x24);
		pos[1] = *(const int16_t *)(b + 0x28);
		pos[2] = *(const int16_t *)(b + 0x2C);
	}

	// WeaponBone: the weapon model's bone (entity +0x78) in the world: through its attach transform (+0x24) and the
	// entity's matrix (+0x40); nothing when the entity has no weapon model
	static void WeaponBone(uint8_t *entity, int32_t bone, int32_t *dir, int16_t *pos)
	{
		uint8_t *w = *(uint8_t **)(entity + 0x78);
		if (!w) return;
		BoneFrame(w, bone, dir, pos);
		Mat4x3 m; // (pad +0x12: ComposeAffineTransform's)
		ComposeAffineTransform((const Mat4x3 *)(entity + 0x40), (const Mat4x3 *)(w + 0x24), &m);
		TransformVectorBy3x3Matrix(&m, dir, dir);
		Normalize(dir, dir);
		MatVec(&m, pos, pos);
		pos[0] = (int16_t)(pos[0] + (int16_t)m.t[0]);
		pos[1] = (int16_t)(pos[1] + (int16_t)m.t[1]);
		pos[2] = (int16_t)(pos[2] + (int16_t)m.t[2]);
	}

	// the trail's draw (TrailDraw) and the spline helpers are used by the held part too: declared here
	static void TrailPoints(TrailWork *w);
	static int32_t TrailGather(const uint8_t *ring, int32_t idx, int32_t want, uint8_t *pa, uint8_t *pb);
	static void TrailSplines(int32_t cnt, const uint8_t *pa, const uint8_t *pb, uint8_t *sa, uint8_t *sb);
	static void TrailDraw(const uint8_t *sa, const uint8_t *sb, TrailWork *w);
