#include "Raven_WeaponSystem.h"
#include "armory/Weapon_RocketLauncher.h"
#include "armory/Weapon_GrenadeLauncher.h"
#include "armory/Weapon_RailGun.h"
#include "armory/Weapon_ShotGun.h"
#include "armory/Weapon_Blaster.h"
#include "Raven_Bot.h"
#include "misc/utils.h"
#include "lua/Raven_Scriptor.h"
#include "Raven_Game.h"
#include "Raven_UserOptions.h"
#include "2D/transformations.h"
#include "misc/Stream_Utility_Functions.h"



//------------------------- ctor ----------------------------------------------
//-----------------------------------------------------------------------------
Raven_WeaponSystem::Raven_WeaponSystem(Raven_Bot* owner,
                                       double ReactionTime,
                                       double AimAccuracy,
                                       double AimPersistance):m_pOwner(owner),
                                                          m_dReactionTime(ReactionTime),
                                                          m_dAimAccuracy(AimAccuracy),
                                                          m_dAimPersistance(AimPersistance)
{
  Initialize();
  InitializeFuzzyModule();
}

//------------------------- dtor ----------------------------------------------
//-----------------------------------------------------------------------------
Raven_WeaponSystem::~Raven_WeaponSystem()
{
  for (unsigned int w=0; w<m_WeaponMap.size(); ++w)
  {
    delete m_WeaponMap[w];
  }
}

//------------------------------ Initialize -----------------------------------
//
//  initializes the weapons
//-----------------------------------------------------------------------------
void Raven_WeaponSystem::Initialize()
{
  //delete any existing weapons
  WeaponMap::iterator curW;
  for (curW = m_WeaponMap.begin(); curW != m_WeaponMap.end(); ++curW)
  {
    delete curW->second;
  }

  m_WeaponMap.clear();

  //set up the container
  m_pCurrentWeapon = new Blaster(m_pOwner);

  m_WeaponMap[type_blaster]         = m_pCurrentWeapon;
  m_WeaponMap[type_shotgun]         = 0;
  m_WeaponMap[type_rail_gun]        = 0;
  m_WeaponMap[type_rocket_launcher] = 0;
  m_WeaponMap[type_grenade_launcher] = 0;
}

//-------------------------------- SelectWeapon -------------------------------
//
//-----------------------------------------------------------------------------
void Raven_WeaponSystem::SelectWeapon()
{ 
  //if a target is present use fuzzy logic to determine the most desirable 
  //weapon.
  if (m_pOwner->GetTargetSys()->isTargetPresent())
  {
    //calculate the distance to the target
    double DistToTarget = Vec2DDistance(m_pOwner->Pos(), m_pOwner->GetTargetSys()->GetTarget()->Pos());

    //for each weapon in the inventory calculate its desirability given the 
    //current situation. The most desirable weapon is selected
    double BestSoFar = MinDouble;

    WeaponMap::const_iterator curWeap;
    for (curWeap=m_WeaponMap.begin(); curWeap != m_WeaponMap.end(); ++curWeap)
    {
      //grab the desirability of this weapon (desirability is based upon
      //distance to target and ammo remaining)
      if (curWeap->second)
      {
        double score = curWeap->second->GetDesirability(DistToTarget);

        //if it is the most desirable so far select it
        if (score > BestSoFar)
        {
          BestSoFar = score;

          //place the weapon in the bot's hand.
          m_pCurrentWeapon = curWeap->second;
        }
      }
    }
  }

  else
  {
    m_pCurrentWeapon = m_WeaponMap[type_blaster];
  }
}

//--------------------  AddWeapon ------------------------------------------
//
//  this is called by a weapon affector and will add a weapon of the specified
//  type to the bot's inventory.
//
//  if the bot already has a weapon of this type then only the ammo is added
//-----------------------------------------------------------------------------
void  Raven_WeaponSystem::AddWeapon(unsigned int weapon_type)
{
  //create an instance of this weapon
  Raven_Weapon* w = 0;

  switch(weapon_type)
  {
  case type_rail_gun:

    w = new RailGun(m_pOwner); break;

  case type_shotgun:

    w = new ShotGun(m_pOwner); break;

  case type_rocket_launcher:

    w = new RocketLauncher(m_pOwner); break;

  case type_grenade_launcher:

    w = new GrenadeLauncher(m_pOwner); break;

  }//end switch
  

  //if the bot already holds a weapon of this type, just add its ammo
  Raven_Weapon* present = GetWeaponFromInventory(weapon_type);

  if (present)
  {
    present->IncrementRounds(w->NumRoundsRemaining());

    delete w;
  }
  
  //if not already holding, add to inventory
  else
  {
    m_WeaponMap[weapon_type] = w;
  }
}


//------------------------- GetWeaponFromInventory -------------------------------
//
//  returns a pointer to any matching weapon.
//
//  returns a null pointer if the weapon is not present
//-----------------------------------------------------------------------------
Raven_Weapon* Raven_WeaponSystem::GetWeaponFromInventory(int weapon_type)
{
  return m_WeaponMap[weapon_type];
}

//----------------------- ChangeWeapon ----------------------------------------
void Raven_WeaponSystem::ChangeWeapon(unsigned int type)
{
  Raven_Weapon* w = GetWeaponFromInventory(type);

  if (w) m_pCurrentWeapon = w;
}

//--------------------------- TakeAimAndShoot ---------------------------------
//
//  this method aims the bots current weapon at the target (if there is a
//  target) and, if aimed correctly, fires a round
//-----------------------------------------------------------------------------
void Raven_WeaponSystem::TakeAimAndShoot()
{
  //aim the weapon only if the current target is shootable or if it has only
  //very recently gone out of view (this latter condition is to ensure the 
  //weapon is aimed at the target even if it temporarily dodges behind a wall
  //or other cover)
  if (m_pOwner->GetTargetSys()->isTargetShootable() ||
      (m_pOwner->GetTargetSys()->GetTimeTargetHasBeenOutOfView() < 
       m_dAimPersistance) )
  {
    //the position the weapon will be aimed at
    Vector2D AimingPos = m_pOwner->GetTargetBot()->Pos();
    
    //if the current weapon is not an instant hit type gun the target position
    //must be adjusted to take into account the predicted movement of the 
    //target
    if (GetCurrentWeapon()->GetType() == type_rocket_launcher ||
		GetCurrentWeapon()->GetType() == type_grenade_launcher ||
        GetCurrentWeapon()->GetType() == type_blaster)
    {
      AimingPos = PredictFuturePositionOfTarget();

      //if the weapon is aimed correctly, there is line of sight between the
      //bot and the aiming position and it has been in view for a period longer
      //than the bot's reaction time, shoot the weapon
      if ( m_pOwner->RotateFacingTowardPosition(AimingPos) &&
           (m_pOwner->GetTargetSys()->GetTimeTargetHasBeenVisible() >
            m_dReactionTime) &&
           m_pOwner->hasLOSto(AimingPos) )
      {
        AddNoiseToAim(AimingPos);

        GetCurrentWeapon()->ShootAt(AimingPos);
      }
    }

    //no need to predict movement, aim directly at target
    else
    {
      //if the weapon is aimed correctly and it has been in view for a period
      //longer than the bot's reaction time, shoot the weapon
      if ( m_pOwner->RotateFacingTowardPosition(AimingPos) &&
           (m_pOwner->GetTargetSys()->GetTimeTargetHasBeenVisible() >
            m_dReactionTime) )
      {
        AddNoiseToAim(AimingPos);
        
        GetCurrentWeapon()->ShootAt(AimingPos);
      }
    }

  }
  
  //no target to shoot at so rotate facing to be parallel with the bot's
  //heading direction
  else
  {
    m_pOwner->RotateFacingTowardPosition(m_pOwner->Pos()+ m_pOwner->Heading());
  }
}

//---------------------------- AddNoiseToAim ----------------------------------
//
//  adds a random deviation to the firing angle not greater than m_dAimAccuracy 
//  rads
//-----------------------------------------------------------------------------
void Raven_WeaponSystem::AddNoiseToAim(Vector2D& AimingPos)const
{
  Vector2D toPos = AimingPos - m_pOwner->Pos();

  Vec2DRotateAroundOrigin(toPos, RandInRange(-m_dAimAccuracy, m_dAimAccuracy));

  AimingPos = toPos + m_pOwner->Pos();
}

//-------------------------  InitializeFuzzyModule ----------------------------
//
//  set up some fuzzy variables and rules
//-----------------------------------------------------------------------------
void Raven_WeaponSystem::InitializeFuzzyModule()
{
	// The shooter deviates its aim accordingly to this fuzzy variable.
	FuzzyVariable& AimDeviation = m_FuzzyModule.CreateFLV("AimDeviation");
	FzSet& ExtraFarLeftAD = AimDeviation.AddLeftShoulderSet("ExtraFarLeftAD", DegsToRads(-90), DegsToRads(-30), DegsToRads(-20));
	FzSet& VeryFarLeftAD = AimDeviation.AddTriangularSet("VeryFarLeftAD", DegsToRads(-30), DegsToRads(-20), DegsToRads(-15));
	FzSet& FarLeftAD = AimDeviation.AddTriangularSet("FarLeftAD", DegsToRads(-20), DegsToRads(-15), DegsToRads(-5));
	FzSet& LeftAD = AimDeviation.AddTriangularSet("LeftAD", DegsToRads(-15), DegsToRads(-5), DegsToRads(0));
	FzSet& CenterAD = AimDeviation.AddTriangularSet("CenterAD", DegsToRads(-5), DegsToRads(0), DegsToRads(5));
	FzSet& RightAD = AimDeviation.AddTriangularSet("RightAD", DegsToRads(0), DegsToRads(5), DegsToRads(15));
	FzSet& FarRightAD = AimDeviation.AddTriangularSet("FarRightAD", DegsToRads(5), DegsToRads(15), DegsToRads(20));
	FzSet& VeryFarRightAD = AimDeviation.AddTriangularSet("VeryFarRightAD", DegsToRads(15), DegsToRads(20), DegsToRads(30));
	FzSet& ExtraFarRightAD = AimDeviation.AddRightShoulderSet("ExtraFarRightAD", DegsToRads(20), DegsToRads(30), DegsToRads(90));

	// Distance between the shooter and its target.
	// In order to avoid too much complexity, the Medium and Far variables are used the same way.
	FuzzyVariable& DistToTarget = m_FuzzyModule.CreateFLV("DistToTarget");
	FzSet& TargetClose = DistToTarget.AddLeftShoulderSet("TargetClose", 0, 25, 150);
	FzSet& TargetMedium = DistToTarget.AddTriangularSet("TargetMedium", 25, 150, 300);
	FzSet& TargetFar = DistToTarget.AddRightShoulderSet("TargetFar", 150, 300, 1000);

	// The speed of the target should stay between 0 and the max value defined in Params.ini
	// We can assume that it will never be greater than 100.
	// In order to avoid too much complexity, we only define 2 sets.
	FuzzyVariable& TargetSpeed = m_FuzzyModule.CreateFLV("TargetSpeed");
	FzSet& TargetSlow = TargetSpeed.AddLeftShoulderSet("TargetSlow", 0.0, 0.25, 0.75);
	FzSet& TargetFast = TargetSpeed.AddRightShoulderSet("TargetFast", 0.25, 0.75, 100.0);

	// After 3 seconds watching its target, the shooter starts to aim better.
	// We can assume that an ennemy will never stay visible or and alive more than 10min.
	// In order to avoid too much complexity, we only define 2 sets.
	FuzzyVariable& VisibilityDuration = m_FuzzyModule.CreateFLV("VisibilityDuration");
	FzSet& ShortPeriod = VisibilityDuration.AddLeftShoulderSet("ShortPeriod", 0, 0, 3);
	FzSet& LongPeriod = VisibilityDuration.AddRightShoulderSet("LongPeriod", 0, 3, 600);

	// Represents the target's heading angle relatively to the shooter's angle of view.
	// In order to be certain that the angle stays in the bounds, we add an extra-degree to them.
	// If the angle is small enough, the target's heading is parallel to the shooter's angle of view : it can just shoot forward without anticipating.
	FuzzyVariable& TargetHeading = m_FuzzyModule.CreateFLV("TargetHeading");
	FzSet& LeftTH = TargetHeading.AddLeftShoulderSet("LeftTH", DegsToRads(-91), DegsToRads(-5), DegsToRads(0));
	FzSet& CenterTH = TargetHeading.AddTriangularSet("CenterTH", DegsToRads(-5), DegsToRads(0), DegsToRads(5));
	FzSet& RightTH = TargetHeading.AddRightShoulderSet("RightTH", DegsToRads(0), DegsToRads(5), DegsToRads(91));



	// Rules :
	//  - The closer the target, the higher the deviation : it's harder to aim accurately a nearby moving target.
	//  - The faster the target, the higher the deviation : a faster target is harder to aim accurately.
	//  - The longer the target is visible, the lower the deviation : once the surprise effect is over, a human is more concentrated over time.
	//  - The deviation is directed on the same side as the heading of the target : its next position is roughly anticipated.
	m_FuzzyModule.AddRule(FzAND(TargetClose, TargetSlow, ShortPeriod, LeftTH), VeryFarLeftAD);
	m_FuzzyModule.AddRule(FzAND(TargetClose, TargetSlow, ShortPeriod, CenterTH), CenterAD);
	m_FuzzyModule.AddRule(FzAND(TargetClose, TargetSlow, ShortPeriod, RightTH), VeryFarRightAD);

	m_FuzzyModule.AddRule(FzAND(TargetClose, TargetSlow, LongPeriod, LeftTH), FarLeftAD);
	m_FuzzyModule.AddRule(FzAND(TargetClose, TargetSlow, LongPeriod, CenterTH), CenterAD);
	m_FuzzyModule.AddRule(FzAND(TargetClose, TargetSlow, LongPeriod, RightTH), FarRightAD);


	m_FuzzyModule.AddRule(FzAND(TargetClose, TargetFast, ShortPeriod, LeftTH), ExtraFarLeftAD);
	m_FuzzyModule.AddRule(FzAND(TargetClose, TargetFast, ShortPeriod, CenterTH), CenterAD);
	m_FuzzyModule.AddRule(FzAND(TargetClose, TargetFast, ShortPeriod, RightTH), ExtraFarRightAD);

	m_FuzzyModule.AddRule(FzAND(TargetClose, TargetFast, LongPeriod, LeftTH), VeryFarLeftAD);
	m_FuzzyModule.AddRule(FzAND(TargetClose, TargetFast, LongPeriod, CenterTH), CenterAD);
	m_FuzzyModule.AddRule(FzAND(TargetClose, TargetFast, LongPeriod, RightTH), VeryFarRightAD);



	m_FuzzyModule.AddRule(FzAND(TargetMedium, TargetSlow, ShortPeriod, LeftTH), FarLeftAD);
	m_FuzzyModule.AddRule(FzAND(TargetMedium, TargetSlow, ShortPeriod, CenterTH), CenterAD);
	m_FuzzyModule.AddRule(FzAND(TargetMedium, TargetSlow, ShortPeriod, RightTH), FarRightAD);

	m_FuzzyModule.AddRule(FzAND(TargetMedium, TargetSlow, LongPeriod, LeftTH), LeftAD);
	m_FuzzyModule.AddRule(FzAND(TargetMedium, TargetSlow, LongPeriod, CenterTH), CenterAD);
	m_FuzzyModule.AddRule(FzAND(TargetMedium, TargetSlow, LongPeriod, RightTH), RightAD);


	m_FuzzyModule.AddRule(FzAND(TargetMedium, TargetFast, ShortPeriod, LeftTH), VeryFarLeftAD);
	m_FuzzyModule.AddRule(FzAND(TargetMedium, TargetFast, ShortPeriod, CenterTH), CenterAD);
	m_FuzzyModule.AddRule(FzAND(TargetMedium, TargetFast, ShortPeriod, RightTH), VeryFarRightAD);

	m_FuzzyModule.AddRule(FzAND(TargetMedium, TargetFast, LongPeriod, LeftTH), FarLeftAD);
	m_FuzzyModule.AddRule(FzAND(TargetMedium, TargetFast, LongPeriod, CenterTH), CenterAD);
	m_FuzzyModule.AddRule(FzAND(TargetMedium, TargetFast, LongPeriod, RightTH), FarRightAD);



	m_FuzzyModule.AddRule(FzAND(TargetFar, TargetSlow, ShortPeriod, LeftTH), FarLeftAD);
	m_FuzzyModule.AddRule(FzAND(TargetFar, TargetSlow, ShortPeriod, CenterTH), CenterAD);
	m_FuzzyModule.AddRule(FzAND(TargetFar, TargetSlow, ShortPeriod, RightTH), FarRightAD);

	m_FuzzyModule.AddRule(FzAND(TargetFar, TargetSlow, LongPeriod, LeftTH), LeftAD);
	m_FuzzyModule.AddRule(FzAND(TargetFar, TargetSlow, LongPeriod, CenterTH), CenterAD);
	m_FuzzyModule.AddRule(FzAND(TargetFar, TargetSlow, LongPeriod, RightTH), RightAD);


	m_FuzzyModule.AddRule(FzAND(TargetFar, TargetFast, ShortPeriod, LeftTH), VeryFarLeftAD);
	m_FuzzyModule.AddRule(FzAND(TargetFar, TargetFast, ShortPeriod, CenterTH), CenterAD);
	m_FuzzyModule.AddRule(FzAND(TargetFar, TargetFast, ShortPeriod, RightTH), VeryFarRightAD);

	m_FuzzyModule.AddRule(FzAND(TargetFar, TargetFast, LongPeriod, LeftTH), FarLeftAD);
	m_FuzzyModule.AddRule(FzAND(TargetFar, TargetFast, LongPeriod, CenterTH), CenterAD);
	m_FuzzyModule.AddRule(FzAND(TargetFar, TargetFast, LongPeriod, RightTH), FarRightAD);
}

//---------------------------- Aim deviation -----------------------------------
//
//-----------------------------------------------------------------------------

double Raven_WeaponSystem::GetAimDeviation()
{
	Vector2D ToEnemy = m_pOwner->GetTargetBot()->Pos() - m_pOwner->Pos(); // Vector from shooter to target.
	Vector2D EnemyHeading = m_pOwner->GetTargetBot()->Heading();
	double dot = EnemyHeading.Dot(Vec2DNormalize(ToEnemy).Perp()); // We take the normalized left perpendicular of the aiming vector (so we can calculate a relevant angle).
	Clamp(dot, -1, 1); // Ensure value remains valid for the acos.
	double angle = acos(dot) - HalfPi; // We substract HalfPi so the final range is [-90, 90].

	m_FuzzyModule.Fuzzify("DistToTarget", ToEnemy.Length());
	m_FuzzyModule.Fuzzify("TargetSpeed", m_pOwner->GetTargetBot()->Speed());
	m_FuzzyModule.Fuzzify("VisibilityDuration", m_pOwner->GetTargetSys()->GetTimeTargetHasBeenVisible());
	m_FuzzyModule.Fuzzify("TargetHeading", angle);

	return m_FuzzyModule.DeFuzzify("AimDeviation", FuzzyModule::max_av);
}

//-------------------------- PredictFuturePositionOfTarget --------------------
//
//  predicts where the target will be located in the time it takes for a
//  projectile to reach it. This uses a similar logic to the Pursuit steering
//  behavior.
//-----------------------------------------------------------------------------
Vector2D Raven_WeaponSystem::PredictFuturePositionOfTarget()
{
  
  Vector2D toPos = m_pOwner->GetTargetBot()->Pos() - m_pOwner->Pos();

  // The direction of the deviation is not orthogonal, so we have to change the sign before applying the rotation.
  Vec2DRotateAroundOrigin(toPos, -GetAimDeviation());

  return toPos + m_pOwner->Pos();

  // We disable the prediction algorithm (which is too accurate)

  /*double MaxSpeed = GetCurrentWeapon()->GetMaxProjectileSpeed();
  
  //if the target is ahead and facing the agent shoot at its current pos
  Vector2D ToEnemy = m_pOwner->GetTargetBot()->Pos() - m_pOwner->Pos();
 
  //the lookahead time is proportional to the distance between the enemy
  //and the pursuer; and is inversely proportional to the sum of the
  //agent's velocities
  double LookAheadTime = ToEnemy.Length() / 
                        (MaxSpeed + m_pOwner->GetTargetBot()->MaxSpeed());
  
  //return the predicted future position of the enemy
  return m_pOwner->GetTargetBot()->Pos() + 
         m_pOwner->GetTargetBot()->Velocity() * LookAheadTime;*/
}


//------------------ GetAmmoRemainingForWeapon --------------------------------
//
//  returns the amount of ammo remaining for the specified weapon. Return zero
//  if the weapon is not present
//-----------------------------------------------------------------------------
int Raven_WeaponSystem::GetAmmoRemainingForWeapon(unsigned int weapon_type)
{
  if (m_WeaponMap[weapon_type])
  {
    return m_WeaponMap[weapon_type]->NumRoundsRemaining();
  }

  return 0;
}

//---------------------------- ShootAt ----------------------------------------
//
//  shoots the current weapon at the given position
//-----------------------------------------------------------------------------
void Raven_WeaponSystem::ShootAt(Vector2D pos)const
{
  GetCurrentWeapon()->ShootAt(pos);
}

//-------------------------- RenderCurrentWeapon ------------------------------
//-----------------------------------------------------------------------------
void Raven_WeaponSystem::RenderCurrentWeapon()const
{
  GetCurrentWeapon()->Render();
}

void Raven_WeaponSystem::RenderDesirabilities()const
{
  Vector2D p = m_pOwner->Pos();

  int num = 0;
  
  WeaponMap::const_iterator curWeap;
  for (curWeap=m_WeaponMap.begin(); curWeap != m_WeaponMap.end(); ++curWeap)
  {
    if (curWeap->second) num++;
  }

  int offset = 15 * num;

    for (curWeap=m_WeaponMap.begin(); curWeap != m_WeaponMap.end(); ++curWeap)
    {
      if (curWeap->second)
      {
        double score = curWeap->second->GetLastDesirabilityScore();
        std::string type = GetNameOfType(curWeap->second->GetType());

        gdi->TextAtPos(p.x+10.0, p.y-offset, ttos(score) + " " + type);

        offset+=15;
      }
    }
}
