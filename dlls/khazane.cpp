/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
#ifndef OEM_BUILD

//=========================================================
// Khazane
//=========================================================
#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"nodes.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"customentity.h"
#include	"weapons.h"
#include	"effects.h"
#include	"soundent.h"
#include	"decals.h"
#include	"explode.h"
#include	"func_break.h"

//=========================================================
// Gargantua Monster
//=========================================================
const float GARG_ATTACKDIST = 80.0;

// Garg animation events
#define GARG_AE_SLASH_LEFT			1
//#define GARG_AE_BEAM_ATTACK_RIGHT	2		// No longer used
#define GARG_AE_LEFT_FOOT			3
#define GARG_AE_RIGHT_FOOT			4
#define GARG_AE_STOMP				5
#define GARG_AE_BREATHE				6


// Gargantua is immune to any damage but this
#define GARG_DAMAGE					(DMG_ENERGYBEAM|DMG_CRUSH|DMG_MORTAR|DMG_BLAST)
#define GARG_EYE_SPRITE_NAME		"sprites/gargeye1.spr"
#define GARG_BEAM_SPRITE_NAME		"sprites/xbeam3.spr"
#define GARG_BEAM_SPRITE2			"sprites/xbeam3.spr"
#define garg_kstomp_SPRITE_NAME		"sprites/gargeye1.spr"
#define garg_kstomp_BUZZ_SOUND		"weapons/mine_charge.wav"
#define GARG_FLAME_LENGTH			330
#define GARG_GIB_MODEL				"models/metalplategibs.mdl"

#define ATTN_GARG					(ATTN_NORM)

#define STOMP_SPRITE_COUNT			10

int gKStompSprite = 0, gKhazaneGibModel = 0;
void SpawnExplosionK(Vector center, float randomRange, float time, int magnitude);

class CKSmoker;

// KSpiral Effect
class CKSpiral : public CBaseEntity
{
public:
	void Spawn(void);
	void Think(void);
	int ObjectCaps(void) { return FCAP_DONT_SAVE; }
	static CKSpiral *Create(const Vector &origin, float height, float radius, float duration);
};
LINK_ENTITY_TO_CLASS(streak_kspiral, CKSpiral);


class CKStomp: public CBaseEntity
{
public:
	void Spawn(void);
	void Think(void);
	static CKStomp*StompCreate(const Vector &origin, const Vector &end, float speed);

private:
	// UNDONE: re-use this sprite list instead of creating new ones all the time
	//	CSprite		*m_pSprites[ STOMP_SPRITE_COUNT ];
};

LINK_ENTITY_TO_CLASS(garg_kstomp, CKStomp);
CKStomp*CKStomp::StompCreate(const Vector &origin, const Vector &end, float speed)
{
	CKStomp*pStomp = GetClassPtr((CKStomp*)NULL);

	pStomp->pev->origin = origin;
	Vector dir = (end - origin);
	pStomp->pev->scale = dir.Length();
	pStomp->pev->movedir = dir.Normalize();
	pStomp->pev->speed = speed;
	pStomp->Spawn();

	return pStomp;
}

void CKStomp::Spawn(void)
{
	pev->nextthink = gpGlobals->time;
	pev->classname = MAKE_STRING("garg_kstomp");
	pev->dmgtime = gpGlobals->time;

	pev->framerate = 30;
	pev->model = MAKE_STRING(garg_kstomp_SPRITE_NAME);
	pev->rendermode = kRenderTransTexture;
	pev->renderamt = 0;
	EMIT_SOUND_DYN(edict(), CHAN_BODY, garg_kstomp_BUZZ_SOUND, 1, ATTN_NORM, 0, PITCH_NORM * 0.55);
}


#define	STOMP_INTERVAL		0.025

void CKStomp::Think(void)
{
	TraceResult tr;

	pev->nextthink = gpGlobals->time + 0.1;

	// Do damage for this frame
	Vector vecStart = pev->origin;
	vecStart.z += 30;
	Vector vecEnd = vecStart + (pev->movedir * pev->speed * gpGlobals->frametime);

	UTIL_TraceHull(vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr);

	if (tr.pHit && tr.pHit != pev->owner)
	{
		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);
		entvars_t *pevOwner = pev;
		if (pev->owner)
			pevOwner = VARS(pev->owner);

		if (pEntity)
			pEntity->TakeDamage(pev, pevOwner, gSkillData.gargantuaDmgStomp, DMG_SONIC);
	}

	// Accelerate the effect
	pev->speed = pev->speed + (gpGlobals->frametime) * pev->framerate;
	pev->framerate = pev->framerate + (gpGlobals->frametime) * 1500;

	// Move and spawn trails
	while (gpGlobals->time - pev->dmgtime > STOMP_INTERVAL)
	{
		pev->origin = pev->origin + pev->movedir * pev->speed * STOMP_INTERVAL;
		for (int i = 0; i < 2; i++)
		{
			CSprite *pSprite = CSprite::SpriteCreate(garg_kstomp_SPRITE_NAME, pev->origin, TRUE);
			if (pSprite)
			{
				UTIL_TraceLine(pev->origin, pev->origin - Vector(0, 0, 500), ignore_monsters, edict(), &tr);
				pSprite->pev->origin = tr.vecEndPos;
				pSprite->pev->velocity = Vector(RANDOM_FLOAT(-200, 200), RANDOM_FLOAT(-200, 200), 175);
				// pSprite->AnimateAndDie( RANDOM_FLOAT( 8.0, 12.0 ) );
				pSprite->pev->nextthink = gpGlobals->time + 0.3;
				pSprite->SetThink(&CSprite::SUB_Remove);
				pSprite->SetTransparency(kRenderTransAdd, 255, 255, 255, 255, kRenderFxFadeFast);
			}
		}
		pev->dmgtime += STOMP_INTERVAL;
		// Scale has the "life" of this effect
		pev->scale -= STOMP_INTERVAL * pev->speed;
		if (pev->scale <= 0)
		{
			// Life has run out
			UTIL_Remove(this);
			STOP_SOUND(edict(), CHAN_BODY, garg_kstomp_BUZZ_SOUND);
		}

	}
}


void KStreakSplash(const Vector &origin, const Vector &direction, int color, int count, int speed, int velocityRange)
{
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, origin);
	WRITE_BYTE(TE_STREAK_SPLASH);
	WRITE_COORD(origin.x);		// origin
	WRITE_COORD(origin.y);
	WRITE_COORD(origin.z);
	WRITE_COORD(direction.x);	// direction
	WRITE_COORD(direction.y);
	WRITE_COORD(direction.z);
	WRITE_BYTE(color);	// Streak color 6
	WRITE_SHORT(count);	// count
	WRITE_SHORT(speed);
	WRITE_SHORT(velocityRange);	// Random velocity modifier
	MESSAGE_END();
}


class CKhazane : public CBaseMonster
{
public:
	void Spawn(void);
	void Precache(void);
	void SetYawSpeed(void);
	int  Classify(void);
	int TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType);
	void TraceAttack(entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType);
	void HandleAnimEvent(MonsterEvent_t *pEvent);

	BOOL CheckMeleeAttack1(float flDot, float flDist);		// Swipe
	BOOL CheckMeleeAttack2(float flDot, float flDist);		// Flames
	BOOL CheckRangeAttack1(float flDot, float flDist);		// Stomp attack
	void SetObjectCollisionBox(void)
	{
		pev->absmin = pev->origin + Vector(-80, -80, 0);
		pev->absmax = pev->origin + Vector(80, 80, 214);
	}

	Schedule_t *GetScheduleOfType(int Type);
	void StartTask(Task_t *pTask);
	void RunTask(Task_t *pTask);

	void PrescheduleThink(void);

	void Killed(entvars_t *pevAttacker, int iGib);
	void DeathEffect(void);

	void EyeOff(void);
	void EyeOn(int level);
	void EyeUpdate(void);
	void Leap(void);
	void StompAttack(void);
	void FlameCreate(void);
	void FlameUpdate(void);
	void FlameControls(float angleX, float angleY);
	void FlameDestroy(void);
	inline BOOL FlameIsOn(void) { return m_pFlame[0] != NULL; }

	void FlameDamage(Vector vecStart, Vector vecEnd, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int iClassIgnore, int bitsDamageType);

	virtual int		Save(CSave &save);
	virtual int		Restore(CRestore &restore);
	static	TYPEDESCRIPTION m_SaveData[];

	CUSTOM_SCHEDULES;

private:
	static const char *pAttackHitSounds[];
	static const char *pBeamAttackSounds[];
	static const char *pAttackMissSounds[];
	static const char *pRicSounds[];
	static const char *pFootSounds[];
	static const char *pIdleSounds[];
	static const char *pAlertSounds[];
	static const char *pPainSounds[];
	static const char *pAttackSounds[];
	static const char *pStompSounds[];
	static const char *pBreatheSounds[];

	CBaseEntity* GargantuaCheckTraceHullAttack(float flDist, int iDamage, int iDmgType);

	CSprite		*m_pEyeGlow;		// Glow around the eyes
	CBeam		*m_pFlame[4];		// Flame beams

	int			m_eyeBrightness;	// Brightness target
	float		m_seeTime;			// Time to attack (when I see the enemy, I set this)
	float		m_flameTime;		// Time of next flame attack
	float		m_painSoundTime;	// Time of next pain sound
	float		m_streakTime;		// streak timer (don't send too many)
	float		m_flameX;			// Flame thrower aim
	float		m_flameY;
};

LINK_ENTITY_TO_CLASS(monster_khazane, CKhazane);

TYPEDESCRIPTION	CKhazane::m_SaveData[] =
{
	DEFINE_FIELD(CKhazane, m_pEyeGlow, FIELD_CLASSPTR),
	DEFINE_FIELD(CKhazane, m_eyeBrightness, FIELD_INTEGER),
	DEFINE_FIELD(CKhazane, m_seeTime, FIELD_TIME),
	DEFINE_FIELD(CKhazane, m_flameTime, FIELD_TIME),
	DEFINE_FIELD(CKhazane, m_streakTime, FIELD_TIME),
	DEFINE_FIELD(CKhazane, m_painSoundTime, FIELD_TIME),
	DEFINE_ARRAY(CKhazane, m_pFlame, FIELD_CLASSPTR, 4),
	DEFINE_FIELD(CKhazane, m_flameX, FIELD_FLOAT),
	DEFINE_FIELD(CKhazane, m_flameY, FIELD_FLOAT),
};

IMPLEMENT_SAVERESTORE(CKhazane, CBaseMonster);

const char *CKhazane::pAttackHitSounds[] =
{
	"zombie/claw_strike1.wav",
	"zombie/claw_strike2.wav",
	"zombie/claw_strike3.wav",
};

const char *CKhazane::pBeamAttackSounds[] =
{
	"garg/gar_flameoff1.wav",
	"garg/gar_flameon1.wav",
	"garg/gar_flamerun1.wav",
};


const char *CKhazane::pAttackMissSounds[] =
{
	"zombie/claw_miss1.wav",
	"zombie/claw_miss2.wav",
};

const char *CKhazane::pRicSounds[] =
{
#if 0
	"weapons/ric1.wav",
	"weapons/ric2.wav",
	"weapons/ric3.wav",
	"weapons/ric4.wav",
	"weapons/ric5.wav",
#else
	"debris/metal4.wav",
	"debris/metal6.wav",
	"weapons/ric4.wav",
	"weapons/ric5.wav",
#endif
};

const char *CKhazane::pFootSounds[] =
{
	"garg/gar_step1.wav",
	"garg/gar_step2.wav",
};


const char *CKhazane::pIdleSounds[] =
{
	"garg/gar_idle1.wav",
	"garg/gar_idle2.wav",
	"garg/gar_idle3.wav",
	"garg/gar_idle4.wav",
	"garg/gar_idle5.wav",
};


const char *CKhazane::pAttackSounds[] =
{
	"garg/gar_attack1.wav",
	"garg/gar_attack2.wav",
	"garg/gar_attack3.wav",
};

const char *CKhazane::pAlertSounds[] =
{
	"garg/gar_alert1.wav",
	"garg/gar_alert2.wav",
	"garg/gar_alert3.wav",
};

const char *CKhazane::pPainSounds[] =
{
	"garg/gar_pain1.wav",
	"garg/gar_pain2.wav",
	"garg/gar_pain3.wav",
};

const char *CKhazane::pStompSounds[] =
{
	"garg/gar_stomp1.wav",
};

const char *CKhazane::pBreatheSounds[] =
{
	"garg/gar_breathe1.wav",
	"garg/gar_breathe2.wav",
	"garg/gar_breathe3.wav",
};
//=========================================================
// AI Schedules Specific to this monster
//=========================================================
#if 0
enum
{
	SCHED_ = LAST_COMMON_SCHEDULE + 1,
};
#endif

enum
{
	TASK_SOUND_ATTACK = LAST_COMMON_TASK + 1,
	TASK_FLAME_SWEEP,
};

Task_t	tlKGargFlame[] =
{
	{ TASK_STOP_MOVING, (float)0 },
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_SOUND_ATTACK, (float)0 },
	// { TASK_PLAY_SEQUENCE,		(float)ACT_SIGNAL1	},
	{ TASK_SET_ACTIVITY, (float)ACT_MELEE_ATTACK2 },
	{ TASK_FLAME_SWEEP, (float)4.5 },
	{ TASK_SET_ACTIVITY, (float)ACT_IDLE },
};

Schedule_t	slKGargFlame[] =
{
	{
		tlKGargFlame,
		ARRAYSIZE(tlKGargFlame),
		0,
		0,
		"GargFlame"
	},
};


// primary melee attack
Task_t	tlKGargSwipe[] =
{
	{ TASK_STOP_MOVING, 0 },
	{ TASK_FACE_ENEMY, (float)0 },
	{ TASK_MELEE_ATTACK1, (float)0 },
};

Schedule_t	slKGargSwipe[] =
{
	{
		tlKGargSwipe,
		ARRAYSIZE(tlKGargSwipe),
		bits_COND_CAN_MELEE_ATTACK2,
		0,
		"GargSwipe"
	},
};


DEFINE_CUSTOM_SCHEDULES(CKhazane)
{
	slKGargFlame,
		slKGargSwipe,
};

IMPLEMENT_CUSTOM_SCHEDULES(CKhazane, CBaseMonster);


void CKhazane::EyeOn(int level)
{
	m_eyeBrightness = level;
}


void CKhazane::EyeOff(void)
{
	m_eyeBrightness = 0;
}


void CKhazane::EyeUpdate(void)
{
	if (m_pEyeGlow)
	{
		m_pEyeGlow->pev->renderamt = UTIL_Approach(m_eyeBrightness, m_pEyeGlow->pev->renderamt, 26);
		if (m_pEyeGlow->pev->renderamt == 0)
			m_pEyeGlow->pev->effects |= EF_NODRAW;
		else
			m_pEyeGlow->pev->effects &= ~EF_NODRAW;
		UTIL_SetOrigin(m_pEyeGlow->pev, pev->origin);
	}
}


void CKhazane::StompAttack(void)
{
	TraceResult trace;

	UTIL_MakeVectors(pev->angles);
	Vector vecStart = pev->origin + Vector(0, 0, 60) + 35 * gpGlobals->v_forward;
	Vector vecAim = ShootAtEnemy(vecStart);
	Vector vecEnd = (vecAim * 1024) + vecStart;

	UTIL_TraceLine(vecStart, vecEnd, ignore_monsters, edict(), &trace);
	CKStomp::StompCreate(vecStart, trace.vecEndPos, 0);
	UTIL_ScreenShake(pev->origin, 12.0, 100.0, 2.0, 1000);
	EMIT_SOUND_DYN(edict(), CHAN_WEAPON, pStompSounds[RANDOM_LONG(0, ARRAYSIZE(pStompSounds) - 1)], 1.0, ATTN_GARG, 0, PITCH_NORM + RANDOM_LONG(-10, 10));

	UTIL_TraceLine(pev->origin, pev->origin - Vector(0, 0, 20), ignore_monsters, edict(), &trace);
	if (trace.flFraction < 1.0)
		UTIL_DecalTrace(&trace, DECAL_GARGSTOMP1);
}


void CKhazane::FlameCreate(void)
{
	int			i;
	Vector		posGun, angleGun;
	TraceResult trace;

	UTIL_MakeVectors(pev->angles);

	for (i = 0; i < 4; i++)
	{
		if (i < 2)
			m_pFlame[i] = CBeam::BeamCreate(GARG_BEAM_SPRITE_NAME, 240);
		else
			m_pFlame[i] = CBeam::BeamCreate(GARG_BEAM_SPRITE2, 140);
		if (m_pFlame[i])
		{
			int attach = i % 2;
			// attachment is 0 based in GetAttachment
			GetAttachment(attach + 1, posGun, angleGun);

			Vector vecEnd = (gpGlobals->v_forward * GARG_FLAME_LENGTH) + posGun;
			UTIL_TraceLine(posGun, vecEnd, dont_ignore_monsters, edict(), &trace);

			m_pFlame[i]->PointEntInit(trace.vecEndPos, entindex());
			if (i < 2)
				m_pFlame[i]->SetColor(255, 130, 90);
			else
				m_pFlame[i]->SetColor(0, 120, 255);
			m_pFlame[i]->SetBrightness(190);
			m_pFlame[i]->SetFlags(BEAM_FSHADEIN);
			m_pFlame[i]->SetScrollRate(20);
			// attachment is 1 based in SetEndAttachment
			m_pFlame[i]->SetEndAttachment(attach + 2);
			CSoundEnt::InsertSound(bits_SOUND_COMBAT, posGun, 384, 0.3);
		}
	}
	EMIT_SOUND_DYN(edict(), CHAN_BODY, pBeamAttackSounds[1], 1.0, ATTN_NORM, 0, PITCH_NORM);
	EMIT_SOUND_DYN(edict(), CHAN_WEAPON, pBeamAttackSounds[2], 1.0, ATTN_NORM, 0, PITCH_NORM);
}


void CKhazane::FlameControls(float angleX, float angleY)
{
	if (angleY < -180)
		angleY += 360;
	else if (angleY > 180)
		angleY -= 360;

	if (angleY < -45)
		angleY = -45;
	else if (angleY > 45)
		angleY = 45;

	m_flameX = UTIL_ApproachAngle(angleX, m_flameX, 4);
	m_flameY = UTIL_ApproachAngle(angleY, m_flameY, 8);
	SetBoneController(0, m_flameY);
	SetBoneController(1, m_flameX);
}


void CKhazane::FlameUpdate(void)
{
	int				i;
	static float	offset[2] = { 60, -60 };
	TraceResult		trace;
	Vector			vecStart, angleGun;
	BOOL			streaks = FALSE;

	for (i = 0; i < 2; i++)
	{
		if (m_pFlame[i])
		{
			Vector vecAim = pev->angles;
			vecAim.x += m_flameX;
			vecAim.y += m_flameY;

			UTIL_MakeVectors(vecAim);

			GetAttachment(i + 1, vecStart, angleGun);
			Vector vecEnd = vecStart + (gpGlobals->v_forward * GARG_FLAME_LENGTH); //  - offset[i] * gpGlobals->v_right;

			UTIL_TraceLine(vecStart, vecEnd, dont_ignore_monsters, edict(), &trace);

			m_pFlame[i]->SetStartPos(trace.vecEndPos);
			m_pFlame[i + 2]->SetStartPos((vecStart * 0.6) + (trace.vecEndPos * 0.4));

			if (trace.flFraction != 1.0 && gpGlobals->time > m_streakTime)
			{
				KStreakSplash(trace.vecEndPos, trace.vecPlaneNormal, 6, 20, 50, 400);
				streaks = TRUE;
				UTIL_DecalTrace(&trace, DECAL_SMALLSCORCH1 + RANDOM_LONG(0, 2));
			}
			// RadiusDamage( trace.vecEndPos, pev, pev, gSkillData.gargantuaDmgFire, CLASS_ALIEN_MONSTER, DMG_BURN );
			FlameDamage(vecStart, trace.vecEndPos, pev, pev, gSkillData.gargantuaDmgFire, CLASS_ALIEN_MONSTER, DMG_BURN);

			MESSAGE_BEGIN(MSG_BROADCAST, SVC_TEMPENTITY);
			WRITE_BYTE(TE_ELIGHT);
			WRITE_SHORT(entindex() + 0x1000 * (i + 2));		// entity, attachment
			WRITE_COORD(vecStart.x);		// origin
			WRITE_COORD(vecStart.y);
			WRITE_COORD(vecStart.z);
			WRITE_COORD(RANDOM_FLOAT(32, 48));	// radius
			WRITE_BYTE(255);	// R
			WRITE_BYTE(255);	// G
			WRITE_BYTE(255);	// B
			WRITE_BYTE(2);	// life * 10
			WRITE_COORD(0); // decay
			MESSAGE_END();
		}
	}
	if (streaks)
		m_streakTime = gpGlobals->time;
}



void CKhazane::FlameDamage(Vector vecStart, Vector vecEnd, entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int iClassIgnore, int bitsDamageType)
{
	CBaseEntity *pEntity = NULL;
	TraceResult	tr;
	float		flAdjustedDamage;
	Vector		vecSpot;

	Vector vecMid = (vecStart + vecEnd) * 0.5;

	float searchRadius = (vecStart - vecMid).Length();

	Vector vecAim = (vecEnd - vecStart).Normalize();

	// iterate on all entities in the vicinity.
	while ((pEntity = UTIL_FindEntityInSphere(pEntity, vecMid, searchRadius)) != NULL)
	{
		if (pEntity->pev->takedamage != DAMAGE_NO)
		{
			// UNDONE: this should check a damage mask, not an ignore
			if (iClassIgnore != CLASS_NONE && pEntity->Classify() == iClassIgnore)
			{// houndeyes don't hurt other houndeyes with their attack
				continue;
			}

			vecSpot = pEntity->BodyTarget(vecMid);

			float dist = DotProduct(vecAim, vecSpot - vecMid);
			if (dist > searchRadius)
				dist = searchRadius;
			else if (dist < -searchRadius)
				dist = searchRadius;

			Vector vecSrc = vecMid + dist * vecAim;

			UTIL_TraceLine(vecSrc, vecSpot, dont_ignore_monsters, ENT(pev), &tr);

			if (tr.flFraction == 1.0 || tr.pHit == pEntity->edict())
			{// the explosion can 'see' this entity, so hurt them!
				// decrease damage for an ent that's farther from the flame.
				dist = (vecSrc - tr.vecEndPos).Length();

				if (dist > 64)
				{
					flAdjustedDamage = flDamage - (dist - 64) * 0.4;
					if (flAdjustedDamage <= 0)
						continue;
				}
				else
				{
					flAdjustedDamage = flDamage;
				}

				// ALERT( at_console, "hit %s\n", STRING( pEntity->pev->classname ) );
				if (tr.flFraction != 1.0)
				{
					ClearMultiDamage();
					pEntity->TraceAttack(pevInflictor, flAdjustedDamage, (tr.vecEndPos - vecSrc).Normalize(), &tr, bitsDamageType);
					ApplyMultiDamage(pevInflictor, pevAttacker);
				}
				else
				{
					pEntity->TakeDamage(pevInflictor, pevAttacker, flAdjustedDamage, bitsDamageType);
				}
			}
		}
	}
}


void CKhazane::FlameDestroy(void)
{
	int i;

	EMIT_SOUND_DYN(edict(), CHAN_WEAPON, pBeamAttackSounds[0], 1.0, ATTN_NORM, 0, PITCH_NORM);
	for (i = 0; i < 4; i++)
	{
		if (m_pFlame[i])
		{
			UTIL_Remove(m_pFlame[i]);
			m_pFlame[i] = NULL;
		}
	}
}


void CKhazane::PrescheduleThink(void)
{
	if (!HasConditions(bits_COND_SEE_ENEMY))
	{
		m_seeTime = gpGlobals->time + 5;
		EyeOff();
	}
	else
		EyeOn(200);

	EyeUpdate();
}


//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int	CKhazane::Classify(void)
{
	return	CLASS_ALIEN_MONSTER;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CKhazane::SetYawSpeed(void)
{
	int ys;

	switch (m_Activity)
	{
	case ACT_IDLE:
		ys = 60;
		break;
	case ACT_TURN_LEFT:
	case ACT_TURN_RIGHT:
		ys = 180;
		break;
	case ACT_WALK:
	case ACT_RUN:
		ys = 60;
		break;

	default:
		ys = 60;
		break;
	}

	pev->yaw_speed = ys;
}


//=========================================================
// Spawn
//=========================================================
void CKhazane::Spawn()
{
	Precache();

	SET_MODEL(ENT(pev), "models/khazane.mdl");
	UTIL_SetSize(pev, Vector(-32, -32, 0), Vector(32, 32, 64));

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	m_bloodColor = BLOOD_COLOR_GREEN;
	pev->health = gSkillData.gargantuaHealth;
	//pev->view_ofs		= Vector ( 0, 0, 96 );// taken from mdl file
	m_flFieldOfView = -0.2;// width of forward view cone ( as a dotproduct result )
	m_MonsterState = MONSTERSTATE_NONE;

	MonsterInit();

	m_pEyeGlow = CSprite::SpriteCreate(GARG_EYE_SPRITE_NAME, pev->origin, FALSE);
	m_pEyeGlow->SetTransparency(kRenderGlow, 255, 255, 255, 0, kRenderFxNoDissipation);
	m_pEyeGlow->SetAttachment(edict(), 1);
	EyeOff();
	m_seeTime = gpGlobals->time + 5;
	m_flameTime = gpGlobals->time + 2;
}


//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CKhazane::Precache()
{
	int i;

	PRECACHE_MODEL("models/khazane.mdl");
	PRECACHE_MODEL(GARG_EYE_SPRITE_NAME);
	PRECACHE_MODEL(GARG_BEAM_SPRITE_NAME);
	PRECACHE_MODEL(GARG_BEAM_SPRITE2);
	gKStompSprite = PRECACHE_MODEL(garg_kstomp_SPRITE_NAME);
	gKhazaneGibModel = PRECACHE_MODEL(GARG_GIB_MODEL);
	PRECACHE_SOUND(garg_kstomp_BUZZ_SOUND);

	for (i = 0; i < ARRAYSIZE(pAttackHitSounds); i++)
		PRECACHE_SOUND((char *)pAttackHitSounds[i]);

	for (i = 0; i < ARRAYSIZE(pBeamAttackSounds); i++)
		PRECACHE_SOUND((char *)pBeamAttackSounds[i]);

	for (i = 0; i < ARRAYSIZE(pAttackMissSounds); i++)
		PRECACHE_SOUND((char *)pAttackMissSounds[i]);

	for (i = 0; i < ARRAYSIZE(pRicSounds); i++)
		PRECACHE_SOUND((char *)pRicSounds[i]);

	for (i = 0; i < ARRAYSIZE(pFootSounds); i++)
		PRECACHE_SOUND((char *)pFootSounds[i]);

	for (i = 0; i < ARRAYSIZE(pIdleSounds); i++)
		PRECACHE_SOUND((char *)pIdleSounds[i]);

	for (i = 0; i < ARRAYSIZE(pAlertSounds); i++)
		PRECACHE_SOUND((char *)pAlertSounds[i]);

	for (i = 0; i < ARRAYSIZE(pPainSounds); i++)
		PRECACHE_SOUND((char *)pPainSounds[i]);

	for (i = 0; i < ARRAYSIZE(pAttackSounds); i++)
		PRECACHE_SOUND((char *)pAttackSounds[i]);

	for (i = 0; i < ARRAYSIZE(pStompSounds); i++)
		PRECACHE_SOUND((char *)pStompSounds[i]);

	for (i = 0; i < ARRAYSIZE(pBreatheSounds); i++)
		PRECACHE_SOUND((char *)pBreatheSounds[i]);
}


void CKhazane::TraceAttack(entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType)
{
	ALERT(at_aiconsole, "CKhazane::TraceAttack\n");

	if (!IsAlive())
	{
		CBaseMonster::TraceAttack(pevAttacker, flDamage, vecDir, ptr, bitsDamageType);
		return;
	}

	// UNDONE: Hit group specific damage?
	if (bitsDamageType & (GARG_DAMAGE | DMG_BLAST))
	{
		if (m_painSoundTime < gpGlobals->time)
		{
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, pPainSounds[RANDOM_LONG(0, ARRAYSIZE(pPainSounds) - 1)], 1.0, ATTN_GARG, 0, PITCH_NORM);
			m_painSoundTime = gpGlobals->time + RANDOM_FLOAT(2.5, 4);
		}
	}

	bitsDamageType &= GARG_DAMAGE;

	if (bitsDamageType == 0)
	{
		if (pev->dmgtime != gpGlobals->time || (RANDOM_LONG(0, 100) < 20))
		{
			UTIL_Ricochet(ptr->vecEndPos, RANDOM_FLOAT(0.5, 1.5));
			pev->dmgtime = gpGlobals->time;
			//			if ( RANDOM_LONG(0,100) < 25 )
			//				EMIT_SOUND_DYN( ENT(pev), CHAN_BODY, pRicSounds[ RANDOM_LONG(0,ARRAYSIZE(pRicSounds)-1) ], 1.0, ATTN_NORM, 0, PITCH_NORM );
		}
		flDamage = 0;
	}

	CBaseMonster::TraceAttack(pevAttacker, flDamage, vecDir, ptr, bitsDamageType);

}



int CKhazane::TakeDamage(entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType)
{
	ALERT(at_aiconsole, "CKhazane::TakeDamage\n");

	if (IsAlive())
	{
		if (!(bitsDamageType & GARG_DAMAGE))
			flDamage *= 0.01;
		if (bitsDamageType & DMG_BLAST)
			SetConditions(bits_COND_LIGHT_DAMAGE);
	}

	return CBaseMonster::TakeDamage(pevInflictor, pevAttacker, flDamage, bitsDamageType);
}


void CKhazane::DeathEffect(void)
{
	int i;
	UTIL_MakeVectors(pev->angles);
	Vector deathPos = pev->origin + gpGlobals->v_forward * 100;

	// Create a spiral of streaks
	CKSpiral::Create(deathPos, (pev->absmax.z - pev->absmin.z) * 0.6, 125, 1.5);

	Vector position = pev->origin;
	position.z += 32;
	for (i = 0; i < 7; i += 2)
	{
		SpawnExplosionK(position, 70, (i * 0.3), 60 + (i * 20));
		position.z += 15;
	}

	CBaseEntity *pSmoker = CBaseEntity::Create("env_smoker", pev->origin, g_vecZero, NULL);
	pSmoker->pev->health = 1;	// 1 smoke balls
	pSmoker->pev->scale = 46;	// 4.6X normal size
	pSmoker->pev->dmg = 0;		// 0 radial distribution
	pSmoker->pev->nextthink = gpGlobals->time + 2.5;	// Start in 2.5 seconds
}


void CKhazane::Killed(entvars_t *pevAttacker, int iGib)
{
	EyeOff();
	UTIL_Remove(m_pEyeGlow);
	m_pEyeGlow = NULL;
	CBaseMonster::Killed(pevAttacker, GIB_NEVER);
}

//=========================================================
// CheckMeleeAttack1
// Garg swipe attack
// 
//=========================================================
BOOL CKhazane::CheckMeleeAttack1(float flDot, float flDist)
{
	//	ALERT(at_aiconsole, "CheckMelee(%f, %f)\n", flDot, flDist);

	if (flDot >= 0.7)
	{
		if (flDist <= GARG_ATTACKDIST)
			return TRUE;
	}
	return FALSE;
}


// Flame thrower madness!
BOOL CKhazane::CheckMeleeAttack2(float flDot, float flDist)
{
	//	ALERT(at_aiconsole, "CheckMelee(%f, %f)\n", flDot, flDist);

	if (gpGlobals->time > m_flameTime)
	{
		if (flDot >= 0.8 && flDist > GARG_ATTACKDIST)
		{
			if (flDist <= GARG_FLAME_LENGTH)
				return TRUE;
		}
	}
	return FALSE;
}


//=========================================================
// CheckRangeAttack1
// flDot is the cos of the angle of the cone within which
// the attack can occur.
//=========================================================
//
// Stomp attack
//
//=========================================================
BOOL CKhazane::CheckRangeAttack1(float flDot, float flDist)
{
	if (gpGlobals->time > m_seeTime)
	{
		if (flDot >= 0.7 && flDist > GARG_ATTACKDIST)
		{
			return TRUE;
		}
	}
	return FALSE;
}




//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CKhazane::HandleAnimEvent(MonsterEvent_t *pEvent)
{
	switch (pEvent->event)
	{
	case GARG_AE_SLASH_LEFT:
	{
							   // HACKHACK!!!
							   CBaseEntity *pHurt = GargantuaCheckTraceHullAttack(GARG_ATTACKDIST + 10.0, gSkillData.gargantuaDmgSlash, DMG_SLASH);
							   if (pHurt)
							   {
								   if (pHurt->pev->flags & (FL_MONSTER | FL_CLIENT))
								   {
									   pHurt->pev->punchangle.x = -30; // pitch
									   pHurt->pev->punchangle.y = -30;	// yaw
									   pHurt->pev->punchangle.z = 30;	// roll
									   //UTIL_MakeVectors(pev->angles);	// called by CheckTraceHullAttack
									   pHurt->pev->velocity = pHurt->pev->velocity - gpGlobals->v_right * 100;
								   }
								   EMIT_SOUND_DYN(edict(), CHAN_WEAPON, pAttackHitSounds[RANDOM_LONG(0, ARRAYSIZE(pAttackHitSounds) - 1)], 1.0, ATTN_NORM, 0, 50 + RANDOM_LONG(0, 15));
							   }
							   else // Play a random attack miss sound
								   EMIT_SOUND_DYN(edict(), CHAN_WEAPON, pAttackMissSounds[RANDOM_LONG(0, ARRAYSIZE(pAttackMissSounds) - 1)], 1.0, ATTN_NORM, 0, 50 + RANDOM_LONG(0, 15));

							   Vector forward;
							   UTIL_MakeVectorsPrivate(pev->angles, forward, NULL, NULL);
	}
		break;

	case GARG_AE_RIGHT_FOOT:
	case GARG_AE_LEFT_FOOT:
		UTIL_ScreenShake(pev->origin, 4.0, 3.0, 1.0, 750);
		EMIT_SOUND_DYN(edict(), CHAN_BODY, pFootSounds[RANDOM_LONG(0, ARRAYSIZE(pFootSounds) - 1)], 1.0, ATTN_GARG, 0, PITCH_NORM + RANDOM_LONG(-10, 10));
		break;

	case GARG_AE_STOMP:
		StompAttack();
		m_seeTime = gpGlobals->time + 12;
		break;

	case GARG_AE_BREATHE:
		EMIT_SOUND_DYN(edict(), CHAN_VOICE, pBreatheSounds[RANDOM_LONG(0, ARRAYSIZE(pBreatheSounds) - 1)], 1.0, ATTN_GARG, 0, PITCH_NORM + RANDOM_LONG(-10, 10));
		break;

	default:
		CBaseMonster::HandleAnimEvent(pEvent);
		break;
	}
}


//=========================================================
// CheckTraceHullAttack - expects a length to trace, amount 
// of damage to do, and damage type. Returns a pointer to
// the damaged entity in case the monster wishes to do
// other stuff to the victim (punchangle, etc)
// Used for many contact-range melee attacks. Bites, claws, etc.

// Overridden for Gargantua because his swing starts lower as
// a percentage of his height (otherwise he swings over the
// players head)
//=========================================================
CBaseEntity* CKhazane::GargantuaCheckTraceHullAttack(float flDist, int iDamage, int iDmgType)
{
	TraceResult tr;

	UTIL_MakeVectors(pev->angles);
	Vector vecStart = pev->origin;
	vecStart.z += 64;
	Vector vecEnd = vecStart + (gpGlobals->v_forward * flDist) - (gpGlobals->v_up * flDist * 0.3);

	UTIL_TraceHull(vecStart, vecEnd, dont_ignore_monsters, head_hull, ENT(pev), &tr);

	if (tr.pHit)
	{
		CBaseEntity *pEntity = CBaseEntity::Instance(tr.pHit);

		if (iDamage > 0)
		{
			pEntity->TakeDamage(pev, pev, iDamage, iDmgType);
		}

		return pEntity;
	}

	return NULL;
}


Schedule_t *CKhazane::GetScheduleOfType(int Type)
{
	// HACKHACK - turn off the flames if they are on and garg goes scripted / dead
	if (FlameIsOn())
		FlameDestroy();

	switch (Type)
	{
	case SCHED_MELEE_ATTACK2:
		return slKGargFlame;
	case SCHED_MELEE_ATTACK1:
		return slKGargSwipe;
		break;
	}

	return CBaseMonster::GetScheduleOfType(Type);
}


void CKhazane::StartTask(Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_FLAME_SWEEP:
		FlameCreate();
		m_flWaitFinished = gpGlobals->time + pTask->flData;
		m_flameTime = gpGlobals->time + 6;
		m_flameX = 0;
		m_flameY = 0;
		break;

	case TASK_SOUND_ATTACK:
		if (RANDOM_LONG(0, 100) < 30)
			EMIT_SOUND_DYN(ENT(pev), CHAN_VOICE, pAttackSounds[RANDOM_LONG(0, ARRAYSIZE(pAttackSounds) - 1)], 1.0, ATTN_GARG, 0, PITCH_NORM);
		TaskComplete();
		break;

	case TASK_DIE:
		m_flWaitFinished = gpGlobals->time + 1.6;
		DeathEffect();
		// FALL THROUGH
	default:
		CBaseMonster::StartTask(pTask);
		break;
	}
}

//=========================================================
// RunTask
//=========================================================
void CKhazane::RunTask(Task_t *pTask)
{
	switch (pTask->iTask)
	{
	case TASK_DIE:
		if (gpGlobals->time > m_flWaitFinished)
		{
			pev->renderfx = kRenderFxExplode;
			pev->rendercolor.x = 255;
			pev->rendercolor.y = 0;
			pev->rendercolor.z = 0;
			StopAnimation();
			pev->nextthink = gpGlobals->time + 0.15;
			SetThink(&CKhazane::SUB_Remove);
			int i;
			int parts = MODEL_FRAMES(gKhazaneGibModel);
			for (i = 0; i < 10; i++)
			{
				CGib *pGib = GetClassPtr((CGib *)NULL);

				pGib->Spawn(GARG_GIB_MODEL);

				int bodyPart = 0;
				if (parts > 1)
					bodyPart = RANDOM_LONG(0, pev->body - 1);

				pGib->pev->body = bodyPart;
				pGib->m_bloodColor = BLOOD_COLOR_YELLOW;
				pGib->m_material = matNone;
				pGib->pev->origin = pev->origin;
				pGib->pev->velocity = UTIL_RandomBloodVector() * RANDOM_FLOAT(300, 500);
				pGib->pev->nextthink = gpGlobals->time + 1.25;
				pGib->SetThink(&CGib::SUB_FadeOut);
			}
			MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
			WRITE_BYTE(TE_BREAKMODEL);

			// position
			WRITE_COORD(pev->origin.x);
			WRITE_COORD(pev->origin.y);
			WRITE_COORD(pev->origin.z);

			// size
			WRITE_COORD(200);
			WRITE_COORD(200);
			WRITE_COORD(128);

			// velocity
			WRITE_COORD(0);
			WRITE_COORD(0);
			WRITE_COORD(0);

			// randomization
			WRITE_BYTE(200);

			// Model
			WRITE_SHORT(gKhazaneGibModel);	//model id#

			// # of shards
			WRITE_BYTE(50);

			// duration
			WRITE_BYTE(20);// 3.0 seconds

			// flags

			WRITE_BYTE(BREAK_FLESH);
			MESSAGE_END();

			return;
		}
		else
			CBaseMonster::RunTask(pTask);
		break;

	case TASK_FLAME_SWEEP:
		if (gpGlobals->time > m_flWaitFinished)
		{
			FlameDestroy();
			TaskComplete();
			FlameControls(0, 0);
			SetBoneController(0, 0);
			SetBoneController(1, 0);
		}
		else
		{
			BOOL cancel = FALSE;

			Vector angles = g_vecZero;

			FlameUpdate();
			CBaseEntity *pEnemy = m_hEnemy;
			if (pEnemy)
			{
				Vector org = pev->origin;
				org.z += 64;
				Vector dir = pEnemy->BodyTarget(org) - org;
				angles = UTIL_VecToAngles(dir);
				angles.x = -angles.x;
				angles.y -= pev->angles.y;
				if (dir.Length() > 400)
					cancel = TRUE;
			}
			if (fabs(angles.y) > 60)
				cancel = TRUE;

			if (cancel)
			{
				m_flWaitFinished -= 0.5;
				m_flameTime -= 0.5;
			}
			// FlameControls( angles.x + 2 * sin(gpGlobals->time*8), angles.y + 28 * sin(gpGlobals->time*8.5) );
			FlameControls(angles.x, angles.y);
		}
		break;

	default:
		CBaseMonster::RunTask(pTask);
		break;
	}
}


class CKSmoker : public CBaseEntity
{
public:
	void Spawn(void);
	void Think(void);
};

LINK_ENTITY_TO_CLASS(env_smokerK, CKSmoker);

void CKSmoker::Spawn(void)
{
	pev->movetype = MOVETYPE_NONE;
	pev->nextthink = gpGlobals->time;
	pev->solid = SOLID_NOT;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->effects |= EF_NODRAW;
	pev->angles = g_vecZero;
}


void CKSmoker::Think(void)
{
	// lots of smoke
	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, pev->origin);
	WRITE_BYTE(TE_SMOKE);
	WRITE_COORD(pev->origin.x + RANDOM_FLOAT(-pev->dmg, pev->dmg));
	WRITE_COORD(pev->origin.y + RANDOM_FLOAT(-pev->dmg, pev->dmg));
	WRITE_COORD(pev->origin.z);
	WRITE_SHORT(g_sModelIndexSmoke);
	WRITE_BYTE(RANDOM_LONG(pev->scale, pev->scale * 1.1));
	WRITE_BYTE(RANDOM_LONG(8, 14)); // framerate
	MESSAGE_END();

	pev->health--;
	if (pev->health > 0)
		pev->nextthink = gpGlobals->time + RANDOM_FLOAT(0.1, 0.2);
	else
		UTIL_Remove(this);
}


void CKSpiral::Spawn(void)
{
	pev->movetype = MOVETYPE_NONE;
	pev->nextthink = gpGlobals->time;
	pev->solid = SOLID_NOT;
	UTIL_SetSize(pev, g_vecZero, g_vecZero);
	pev->effects |= EF_NODRAW;
	pev->angles = g_vecZero;
}


CKSpiral *CKSpiral::Create(const Vector &origin, float height, float radius, float duration)
{
	if (duration <= 0)
		return NULL;

	CKSpiral *pKSpiral = GetClassPtr((CKSpiral *)NULL);
	pKSpiral->Spawn();
	pKSpiral->pev->dmgtime = pKSpiral->pev->nextthink;
	pKSpiral->pev->origin = origin;
	pKSpiral->pev->scale = radius;
	pKSpiral->pev->dmg = height;
	pKSpiral->pev->speed = duration;
	pKSpiral->pev->health = 0;
	pKSpiral->pev->angles = g_vecZero;

	return pKSpiral;
}

#define SPIRAL_INTERVAL		0.1 //025

void CKSpiral::Think(void)
{
	float time = gpGlobals->time - pev->dmgtime;

	while (time > SPIRAL_INTERVAL)
	{
		Vector position = pev->origin;
		Vector direction = Vector(0, 0, 1);

		float fraction = 1.0 / pev->speed;

		float radius = (pev->scale * pev->health) * fraction;

		position.z += (pev->health * pev->dmg) * fraction;
		pev->angles.y = (pev->health * 360 * 8) * fraction;
		UTIL_MakeVectors(pev->angles);
		position = position + gpGlobals->v_forward * radius;
		direction = (direction + gpGlobals->v_forward).Normalize();

		KStreakSplash(position, Vector(0, 0, 1), RANDOM_LONG(8, 11), 20, RANDOM_LONG(50, 150), 400);

		// Jeez, how many counters should this take ? :)
		pev->dmgtime += SPIRAL_INTERVAL;
		pev->health += SPIRAL_INTERVAL;
		time -= SPIRAL_INTERVAL;
	}

	pev->nextthink = gpGlobals->time;

	if (pev->health >= pev->speed)
		UTIL_Remove(this);
}


// HACKHACK Cut and pasted from explode.cpp
void SpawnExplosionK(Vector center, float randomRange, float time, int magnitude)
{
	KeyValueData	kvd;
	char			buf[128];

	center.x += RANDOM_FLOAT(-randomRange, randomRange);
	center.y += RANDOM_FLOAT(-randomRange, randomRange);

	CBaseEntity *pExplosion = CBaseEntity::Create("env_explosion", center, g_vecZero, NULL);
	sprintf(buf, "%3d", magnitude);
	kvd.szKeyName = "iMagnitude";
	kvd.szValue = buf;
	pExplosion->KeyValue(&kvd);
	pExplosion->pev->spawnflags |= SF_ENVEXPLOSION_NODAMAGE;

	pExplosion->Spawn();
	pExplosion->SetThink(&CBaseEntity::SUB_CallUseToggle);
	pExplosion->pev->nextthink = gpGlobals->time + time;
}



#endif
