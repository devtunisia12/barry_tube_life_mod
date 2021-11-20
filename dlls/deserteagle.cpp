#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "weapons.h"
#include "player.h"

// These correspond directly to the sequences in the weapon's view model
enum desert_eagle_e {
	DESERT_EAGLE_IDLE1 = 0,
	DESERT_EAGLE_IDLE2,
	DESERT_EAGLE_IDLE3,
	DESERT_EAGLE_IDLE4,
	DESERT_EAGLE_IDLE5,
	DESERT_EAGLE_SHOOT,
	DESERT_EAGLE_SHOOT_EMPTY,
	DESERT_EAGLE_RELOAD,
	DESERT_EAGLE_RELOAD_NOT_EMPTY,
	DESERT_EAGLE_DRAW,
	DESERT_EAGLE_HOLSTER,
};

LINK_ENTITY_TO_CLASS(weapon_eagle, CDesertEagle)

void CDesertEagle::Spawn()
{
	// Define the classname of the entity
	// This is the name you should use to reference this entity name in your code base.
	pev->classname = MAKE_STRING("weapon_eagle");

	// Precache the weapon models and sounds
	// This might be called by the engine separately, but it's best to call it here as well just in case.
	Precache();

	// Set the weapon ID
	m_iId = WEAPON_DESERT_EAGLE;

	// Tell the engine about the weapon's world model
	SET_MODEL(ENT(pev), "models/w_desert_eagle.mdl");

	// Set the default ammo value for the weapon
	m_iDefaultAmmo = DESERT_EAGLE_DEFAULT_GIVE;

	// Set up some default behaviour for the weapon
	// This will tell the engine that the weapon should "fall" to the ground when it spawns.
	// It also sets up the behaviour so that the weapon is equipped when the player touches it.
	FallInit();
}

void CDesertEagle::Precache()
{
	// Precache models
	PRECACHE_MODEL("models/v_desert_eagle.mdl");
	PRECACHE_MODEL("models/w_desert_eagle.mdl");
	PRECACHE_MODEL("models/p_desert_eagle.mdl");
	PRECACHE_MODEL("models/shell.mdl");

	// Precache sounds
	PRECACHE_SOUND("weapons/desert_eagle_fire.wav");
	PRECACHE_SOUND("weapons/desert_eagle_sight.wav");
	PRECACHE_SOUND("weapons/desert_eagle_sight2.wav");

	// Precache fire event
	m_usFireDesertEagle = PRECACHE_EVENT(1, "events/eagle.sc");
}

int CDesertEagle::GetItemInfo(ItemInfo* p)
{
	// This should match the classname - the HUD uses it to find the matching .txt file in the sprites/ folder
	p->pszName = STRING(pev->classname);

	// The "primary" ammo type for this weapon and the maximum ammo of that type that the player can hold
	p->pszAmmo1 = "357";
	p->iMaxAmmo1 = _357_MAX_CARRY;

	// Same as above, but for "secondary" ammo. This should be NULL and -1 for weapons with no secondary
	p->pszAmmo2 = NULL;
	p->iMaxAmmo2 = -1;

	// The size of a full clip
	p->iMaxClip = DESERT_EAGLE_MAX_CLIP;

	// Special weapon flags - leave this as 0 for now, this is covered in a different article
	p->iFlags = 0;

	// The "slot" in the HUD that the weapon appears in. This is a pistol, so it goes into slot 1 with the others
	p->iSlot = 1;

	// The "position" in the HUD that the weapon is added to. We'll put this after the magnum (which is in slot 1)
	p->iPosition = 2;

	// Set the ID and auto-switching weights of the weapon
	p->iId = m_iId = WEAPON_DESERT_EAGLE;
	p->iWeight = DESERT_EAGLE_WEIGHT;

	return TRUE;
}

int CDesertEagle::AddToPlayer(CBasePlayer* pPlayer)
{
	// AddToPlayer returns TRUE if the weapon was picked up
	if (CBasePlayerWeapon::AddToPlayer(pPlayer))
	{
		// Send a message to the client so the pickup icon can be shown
		MESSAGE_BEGIN(MSG_ONE, gmsgWeapPickup, NULL, pPlayer->pev);
		WRITE_BYTE(m_iId);
		MESSAGE_END();
		return TRUE;
	}
	return FALSE;
}

void CDesertEagle::SecondaryAttack()
{

}

void CDesertEagle::PrimaryAttack()
{
	// Don't fire underwater - waterlevel 3 indicates that the player's head is underwater
	if (m_pPlayer->pev->waterlevel == 3)
	{
		// Play a "click" and don't allow another primary attack for a short time
		PlayEmptySound();
		m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.15;
		return;
	}

	// Check if the clip is empty
	if (m_iClip <= 0)
	{
		if (!m_fInReload && m_fFireOnEmpty)
		{
			// If the player has fired previously, but is still holding the attack button down,
			// just play the empty "click" sound until the player releases the button.
			PlayEmptySound();
			m_flNextPrimaryAttack = UTIL_WeaponTimeBase() + 0.2;
		}

		return;
	}

	// If we get to this point - we're shooting!

	m_pPlayer->m_iWeaponVolume = NORMAL_GUN_VOLUME;
	m_pPlayer->m_iWeaponFlash = NORMAL_GUN_FLASH;

	// Decrease the number of bullets in the clip
	m_iClip--;

	// Add a muzzleflash to the player effects
	m_pPlayer->pev->effects |= EF_MUZZLEFLASH;

	// Player "shoot" animation
	m_pPlayer->SetAnimation(PLAYER_ATTACK1);

	// Set global vectors in the engine (don't ask)
	UTIL_MakeVectors(m_pPlayer->pev->v_angle + m_pPlayer->pev->punchangle);

	// Shoot bullets!
	Vector vecSrc = m_pPlayer->GetGunPosition();
	Vector vecAiming = m_pPlayer->GetAutoaimVector(AUTOAIM_10DEGREES);
	Vector vecDir = m_pPlayer->FireBulletsPlayer(
		1,                     // Number of bullets to shoot
		vecSrc,                // The source of the bullets (i.e. the gun)
		vecAiming,             // The direction to fire in (i.e. where the player is pointing)
		VECTOR_CONE_10DEGREES, // The accuracy spread of the weapon
		8192,                  // The distance the bullet can go (8192 is the limit for the engine)
		BULLET_PLAYER_357,     // The type of bullet being fired
		0,                     // Number of tracer bullets to fire (none in this case)
		0,                     // Set to non-zero to override the amount of damage (usually, leave this as 0)
		m_pPlayer->pev,        // Attacker entity
		m_pPlayer->random_seed // The random seed
		);

	int flags;
#if defined( CLIENT_WEAPONS )
	flags = FEV_NOTHOST;
#else
	flags = 0;
#endif

	PLAYBACK_EVENT_FULL(flags, m_pPlayer->edict(), m_usFireDesertEagle, 0.0, (float*)&g_vecZero, (float*)&g_vecZero, vecDir.x, vecDir.y, 0, 0, (m_iClip == 0) ? 1 : 0, 0);

	// If the clip is now empty and there's no more ammo available, update the HEV
	if (!m_iClip && m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
	{
		// HEV suit - indicate out of ammo condition
		m_pPlayer->SetSuitUpdate("!HEV_AMO0", FALSE, 0);
	}

	// The desert eagle can fire quite quickly with no laser spot, so use a 250ms delay
	m_flNextPrimaryAttack = m_flNextSecondaryAttack = UTIL_WeaponTimeBase() + 0.25;

	// Set the time until the weapon should start idling again
	m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
}

BOOL CDesertEagle::Deploy()
{
	return DefaultDeploy(
		"models/v_desert_eagle.mdl", // Weapon view model
		"models/p_desert_eagle.mdl", // Weapon player model
		DESERT_EAGLE_DRAW, // "Draw" animation index for the view model
		"onehanded",       // Third person animation set for the weapon. We'll use the generic "onehanded" animation set
		UseDecrement(),    // Flag whether or not to do client prediction or not
		pev->body          // The weapon model's "body" pointer
		);
}

void CDesertEagle::Holster(int skiplocal)
{
	// Cancel any reload in progress
	m_fInReload = FALSE;

	// Delay the next player's attack for about the same time as the holster animation takes
	m_pPlayer->m_flNextAttack = UTIL_WeaponTimeBase() + 0.5;

	// Play the "holster" animation
	SendWeaponAnim(DESERT_EAGLE_HOLSTER);
}

void CDesertEagle::Reload()
{
	// Don't reload if the player doesn't have any ammo
	if (m_pPlayer->ammo_357 <= 0) return;

	int iResult;

	// The view model has two different animations depending on if there are any bullets in the clip
	if (m_iClip == 0) iResult = DefaultReload(DESERT_EAGLE_MAX_CLIP, DESERT_EAGLE_RELOAD, 1.5);
	else iResult = DefaultReload(DESERT_EAGLE_MAX_CLIP, DESERT_EAGLE_RELOAD_NOT_EMPTY, 1.5);

	if (iResult)
	{
		// If the reload happened, then reset the weapon's idle time
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + UTIL_SharedRandomFloat(m_pPlayer->random_seed, 10, 15);
	}
}

void CDesertEagle::WeaponIdle()
{
	// This is used in conjunction with the PlayEmptySound function.
	// This resets a flag so the "click" for an empty weapon can be replayed after a short delay
	ResetEmptySound();

	// Despite the name, this will SET the autoaim vector.
	// 10 degrees is what the magnum uses, so we'll use the same.
	m_pPlayer->GetAutoaimVector(AUTOAIM_10DEGREES);

	// Exit out of the method if the weapon time hasn't passed yet or if the clip is empty
	if (m_flTimeWeaponIdle > UTIL_WeaponTimeBase() || m_iClip <= 0) return;

	// Weapon idle is only called after the weapon hasn't been used (fired or reloaded)
	// for a while. In this case we want to play one of the idle animations for the weapon.
	// The desert eagle view model has 5 different idle animations, and we'll give each one
	// a 20% chance of playing, using the random number util function.
	int iAnim;
	float flRand = UTIL_SharedRandomFloat(m_pPlayer->random_seed, 0, 1);

	if (flRand <= 0.2)
	{
		// The numbers here (76.0 / 30.0) are a way to represent the time taken by the
		// animation, so the next idle animation isn't played before the current one has
		// been completed. This animation is 76 frames long, and runs at 30 frames per second.
		iAnim = DESERT_EAGLE_IDLE1;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + (76.0 / 30.0);
	}
	else if (flRand <= 0.4)
	{
		iAnim = DESERT_EAGLE_IDLE2;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + (61.0 / 24.0);
	}
	else if (flRand <= 0.6)
	{
		iAnim = DESERT_EAGLE_IDLE3;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + (50.0 / 30.0);
	}
	else if (flRand <= 0.8)
	{
		iAnim = DESERT_EAGLE_IDLE4;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + (76.0 / 30.0);
	}
	else
	{
		iAnim = DESERT_EAGLE_IDLE5;
		m_flTimeWeaponIdle = UTIL_WeaponTimeBase() + (61.0 / 30.0);
	}

	// Play the idle animation
	SendWeaponAnim(iAnim, UseDecrement(), pev->body);
}