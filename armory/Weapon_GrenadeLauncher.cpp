#include "Weapon_GrenadeLauncher.h"
#include "../Raven_Bot.h"
#include "misc/Cgdi.h"
#include "../Raven_Game.h"
#include "../Raven_Map.h"
#include "../lua/Raven_Scriptor.h"
#include "fuzzy/FuzzyOperators.h"


//--------------------------- ctor --------------------------------------------
//-----------------------------------------------------------------------------
GrenadeLauncher::GrenadeLauncher(Raven_Bot*   owner) :

	Raven_Weapon(type_grenade_launcher,
		script->GetInt("GrenadeLauncher_DefaultRounds"),
		script->GetInt("GrenadeLauncher_MaxRoundsCarried"),
		script->GetDouble("GrenadeLauncher_FiringFreq"),
		script->GetDouble("GrenadeLauncher_IdealRange"),
		script->GetDouble("Grenade_MaxSpeed"),
		owner)
{
	//setup the vertex buffer
	const int NumWeaponVerts = 8;
	const Vector2D weapon[NumWeaponVerts] = {
		Vector2D(0, -3),
		Vector2D(6, -3),
		Vector2D(6, -1),
		Vector2D(15, -1),
		Vector2D(15, 1),
		Vector2D(6, 1),
		Vector2D(6, 3),
		Vector2D(0, 3)
	};

	for (int vtx = 0; vtx<NumWeaponVerts; ++vtx)
	{
		m_vecWeaponVB.push_back(weapon[vtx]);
	}

	//setup the fuzzy module
	InitializeFuzzyModule();

}


//------------------------------ ShootAt --------------------------------------
//-----------------------------------------------------------------------------
inline void GrenadeLauncher::ShootAt(Vector2D pos)
{
	if (NumRoundsRemaining() > 0 && isReadyForNextShot())
	{
		//fire off a grenade!
		m_pOwner->GetWorld()->AddGrenade(m_pOwner, pos);

		m_iNumRoundsLeft--;

		UpdateTimeWeaponIsNextAvailable();

		//add a trigger to the game so that the other bots can hear this shot
		//(provided they are within range)
		m_pOwner->GetWorld()->GetMap()->AddSoundTrigger(m_pOwner, script->GetDouble("GrenadeLauncher_SoundRange"));
	}
}

//---------------------------- Desirability -----------------------------------
//
//-----------------------------------------------------------------------------
double GrenadeLauncher::GetDesirability(double DistToTarget)
{
	if (m_iNumRoundsLeft == 0)
	{
		m_dLastDesirabilityScore = 0;
	}
	else
	{
		//fuzzify distance and amount of ammo
		m_FuzzyModule.Fuzzify("DistToTarget", DistToTarget);
		m_FuzzyModule.Fuzzify("AmmoStatus", (double)m_iNumRoundsLeft);

		m_dLastDesirabilityScore = m_FuzzyModule.DeFuzzify("Desirability", FuzzyModule::max_av);
	}

	return m_dLastDesirabilityScore;
}

//-------------------------  InitializeFuzzyModule ----------------------------
//
//  set up some fuzzy variables and rules
//-----------------------------------------------------------------------------
void GrenadeLauncher::InitializeFuzzyModule()
{
	FuzzyVariable& DistToTarget = m_FuzzyModule.CreateFLV("DistToTarget");
	FzSet& Target_VeryClose = DistToTarget.AddLeftShoulderSet("Target_VeryClose", 0, 25, 150);
	FzSet& Target_Close = DistToTarget.AddLeftShoulderSet("Target_Close", 15, 90, 225);
	FzSet& Target_Medium = DistToTarget.AddTriangularSet("Target_Medium", 25, 150, 300);
	FzSet& Target_Far = DistToTarget.AddRightShoulderSet("Target_Far", 90, 225, 750);
	FzSet& Target_VeryFar = DistToTarget.AddRightShoulderSet("Target_VeryFar", 150, 300, 1000);

	FuzzyVariable& Desirability = m_FuzzyModule.CreateFLV("Desirability");
	FzSet& VeryDesirable = Desirability.AddRightShoulderSet("VeryDesirable", 50, 75, 100);
	FzSet& MoreDesirable = Desirability.AddRightShoulderSet("MoreDesirable", 37, 62, 87);
	FzSet& Desirable = Desirability.AddTriangularSet("Desirable", 25, 50, 75);
	FzSet& LessDesirable = Desirability.AddLeftShoulderSet("LessDesirable", 12, 37, 62);
	FzSet& Undesirable = Desirability.AddLeftShoulderSet("Undesirable", 0, 25, 50);

	FuzzyVariable& AmmoStatus = m_FuzzyModule.CreateFLV("AmmoStatus");
	FzSet& Ammo_Loads = AmmoStatus.AddRightShoulderSet("Ammo_Loads", 10, 30, 100);
	FzSet& Ammo_Good = AmmoStatus.AddRightShoulderSet("Ammo_Good", 5, 20, 65);
	FzSet& Ammo_Okay = AmmoStatus.AddTriangularSet("Ammo_Okay", 0, 10, 30);
	FzSet& Ammo_Low = AmmoStatus.AddTriangularSet("Ammo_Low", 0, 5, 20);
	FzSet& Ammo_VeryLow = AmmoStatus.AddTriangularSet("Ammo_VeryLow", 0, 0, 10);

	m_FuzzyModule.AddRule(FzAND(Target_VeryClose, Ammo_Loads), Undesirable);
	m_FuzzyModule.AddRule(FzAND(Target_VeryClose, Ammo_Good), Undesirable);
	m_FuzzyModule.AddRule(FzAND(Target_VeryClose, Ammo_Okay), Undesirable);
	m_FuzzyModule.AddRule(FzAND(Target_VeryClose, Ammo_Low), Undesirable);
	m_FuzzyModule.AddRule(FzAND(Target_VeryClose, Ammo_VeryLow), Undesirable);

	m_FuzzyModule.AddRule(FzAND(Target_Close, Ammo_Loads), Desirable);
	m_FuzzyModule.AddRule(FzAND(Target_Close, Ammo_Good), Desirable);
	m_FuzzyModule.AddRule(FzAND(Target_Close, Ammo_Okay), LessDesirable);
	m_FuzzyModule.AddRule(FzAND(Target_Close, Ammo_Low), Undesirable);
	m_FuzzyModule.AddRule(FzAND(Target_Close, Ammo_VeryLow), Undesirable);

	m_FuzzyModule.AddRule(FzAND(Target_Medium, Ammo_Loads), VeryDesirable);
	m_FuzzyModule.AddRule(FzAND(Target_Medium, Ammo_Good), VeryDesirable);
	m_FuzzyModule.AddRule(FzAND(Target_Medium, Ammo_Okay), MoreDesirable);
	m_FuzzyModule.AddRule(FzAND(Target_Medium, Ammo_Low), Desirable);
	m_FuzzyModule.AddRule(FzAND(Target_Medium, Ammo_VeryLow), LessDesirable);

	m_FuzzyModule.AddRule(FzAND(Target_Far, Ammo_Loads), MoreDesirable);
	m_FuzzyModule.AddRule(FzAND(Target_Far, Ammo_Good), Desirable);
	m_FuzzyModule.AddRule(FzAND(Target_Far, Ammo_Okay), LessDesirable);
	m_FuzzyModule.AddRule(FzAND(Target_Far, Ammo_Low), Undesirable);
	m_FuzzyModule.AddRule(FzAND(Target_Far, Ammo_VeryLow), Undesirable);

	m_FuzzyModule.AddRule(FzAND(Target_VeryFar, Ammo_Loads), LessDesirable);
	m_FuzzyModule.AddRule(FzAND(Target_VeryFar, Ammo_Good), Undesirable);
	m_FuzzyModule.AddRule(FzAND(Target_VeryFar, Ammo_Okay), Undesirable);
	m_FuzzyModule.AddRule(FzAND(Target_VeryFar, Ammo_Low), Undesirable);
	m_FuzzyModule.AddRule(FzAND(Target_VeryFar, Ammo_VeryLow), Undesirable);
}


//-------------------------------- Render -------------------------------------7
//-----------------------------------------------------------------------------
void GrenadeLauncher::Render()
{
	m_vecWeaponVBTrans = WorldTransform(m_vecWeaponVB,
		m_pOwner->Pos(),
		m_pOwner->Facing(),
		m_pOwner->Facing().Perp(),
		m_pOwner->Scale());

	gdi->RedPen();

	gdi->ClosedShape(m_vecWeaponVBTrans);
}
