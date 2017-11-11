#include "Weapon_RocketLauncher.h"
#include "../Raven_Bot.h"
#include "misc/Cgdi.h"
#include "../Raven_Game.h"
#include "../Raven_Map.h"
#include "../lua/Raven_Scriptor.h"
#include "fuzzy/FuzzyOperators.h"


//--------------------------- ctor --------------------------------------------
//-----------------------------------------------------------------------------
RocketLauncher::RocketLauncher(Raven_Bot*   owner):

                      Raven_Weapon(type_rocket_launcher,
                                   script->GetInt("RocketLauncher_DefaultRounds"),
                                   script->GetInt("RocketLauncher_MaxRoundsCarried"),
                                   script->GetDouble("RocketLauncher_FiringFreq"),
                                   script->GetDouble("RocketLauncher_IdealRange"),
                                   script->GetDouble("Rocket_MaxSpeed"),
                                   owner)
{
    //setup the vertex buffer
  const int NumWeaponVerts = 8;
  const Vector2D weapon[NumWeaponVerts] = {Vector2D(0, -3),
                                           Vector2D(6, -3),
                                           Vector2D(6, -1),
                                           Vector2D(15, -1),
                                           Vector2D(15, 1),
                                           Vector2D(6, 1),
                                           Vector2D(6, 3),
                                           Vector2D(0, 3)
                                           };
  for (int vtx=0; vtx<NumWeaponVerts; ++vtx)
  {
    m_vecWeaponVB.push_back(weapon[vtx]);
  }

  //setup the fuzzy module
  InitializeFuzzyModule();

}


//------------------------------ ShootAt --------------------------------------
//-----------------------------------------------------------------------------
inline void RocketLauncher::ShootAt(Vector2D pos)
{ 
  if (NumRoundsRemaining() > 0 && isReadyForNextShot())
  {
    //fire off a rocket!
    m_pOwner->GetWorld()->AddRocket(m_pOwner, pos);

    m_iNumRoundsLeft--;

    UpdateTimeWeaponIsNextAvailable();

    //add a trigger to the game so that the other bots can hear this shot
    //(provided they are within range)
    m_pOwner->GetWorld()->GetMap()->AddSoundTrigger(m_pOwner, script->GetDouble("RocketLauncher_SoundRange"));
  }
}

//---------------------------- Desirability -----------------------------------
//
//-----------------------------------------------------------------------------
double RocketLauncher::GetDesirability(double DistToTarget)
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
void RocketLauncher::InitializeFuzzyModule()
{
  FuzzyVariable& DistToTarget = m_FuzzyModule.CreateFLV("DistToTarget");

  FzSet& Target_Close = DistToTarget.AddLeftShoulderSet("Target_Close",0,25,87.5);
  FzSet& Target_CloseMedium = DistToTarget.AddTriangularSet("Target_CloseMedium", 25, 87.5, 150);
  FzSet& Target_Medium = DistToTarget.AddTriangularSet("Target_Medium",87.5,150,225);
  FzSet& Target_MediumFar = DistToTarget.AddTriangularSet("Target_MediumFar", 150, 225, 300);
  FzSet& Target_Far = DistToTarget.AddRightShoulderSet("Target_Far",225,300,1000);

  FuzzyVariable& Desirability = m_FuzzyModule.CreateFLV("Desirability"); 
  FzSet& Undesirable = Desirability.AddLeftShoulderSet("Undesirable", 0, 12.5, 25);
  FzSet& LessDesirable = Desirability.AddTriangularSet("LessDesirable", 12.5, 25, 50);
  FzSet& Desirable = Desirability.AddTriangularSet("Desirable", 25, 50, 75);
  FzSet& MoreDesirable = Desirability.AddTriangularSet("MoreDesirable", 50, 75, 87.5);
  FzSet& VeryDesirable = Desirability.AddRightShoulderSet("VeryDesirable", 75, 87.5, 100);

  FuzzyVariable& AmmoStatus = m_FuzzyModule.CreateFLV("AmmoStatus");
  FzSet& Ammo_VeryLow = AmmoStatus.AddTriangularSet("Ammo_VeryLow", 0, 0, 10);
  FzSet& Ammo_Low = AmmoStatus.AddTriangularSet("Ammo_Low", 0, 10, 30);
  FzSet& Ammo_Okay = AmmoStatus.AddTriangularSet("Ammo_Okay", 10, 30, 50);
  FzSet& Ammo_Preferable = AmmoStatus.AddTriangularSet("Ammo_Preferable", 30, 50, 70);
  FzSet& Ammo_Loads = AmmoStatus.AddRightShoulderSet("Ammo_Loads", 50, 70, 100);


  m_FuzzyModule.AddRule(FzAND(Target_Close, Ammo_VeryLow), Undesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Close, Ammo_Low), Undesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Close, Ammo_Okay), Undesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Close, Ammo_Preferable), Undesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Close, Ammo_Loads), Undesirable);  

  m_FuzzyModule.AddRule(FzAND(Target_CloseMedium, Ammo_VeryLow), LessDesirable);
  m_FuzzyModule.AddRule(FzAND(Target_CloseMedium, Ammo_Low), LessDesirable);
  m_FuzzyModule.AddRule(FzAND(Target_CloseMedium, Ammo_Okay), LessDesirable);
  m_FuzzyModule.AddRule(FzAND(Target_CloseMedium, Ammo_Preferable), LessDesirable);
  m_FuzzyModule.AddRule(FzAND(Target_CloseMedium, Ammo_Loads), LessDesirable);

  m_FuzzyModule.AddRule(FzAND(Target_Medium, Ammo_VeryLow), LessDesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Medium, Ammo_Low), Desirable);
  m_FuzzyModule.AddRule(FzAND(Target_Medium, Ammo_Okay), MoreDesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Medium, Ammo_Preferable), VeryDesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Medium, Ammo_Loads), VeryDesirable);

  m_FuzzyModule.AddRule(FzAND(Target_MediumFar, Ammo_VeryLow), LessDesirable);
  m_FuzzyModule.AddRule(FzAND(Target_MediumFar, Ammo_Low), Desirable);
  m_FuzzyModule.AddRule(FzAND(Target_MediumFar, Ammo_Okay), MoreDesirable);
  m_FuzzyModule.AddRule(FzAND(Target_MediumFar, Ammo_Preferable), VeryDesirable);
  m_FuzzyModule.AddRule(FzAND(Target_MediumFar, Ammo_Loads), VeryDesirable);

  m_FuzzyModule.AddRule(FzAND(Target_Far, Ammo_VeryLow), Undesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Far, Ammo_Low), Undesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Far, Ammo_Okay), LessDesirable);
  m_FuzzyModule.AddRule(FzAND(Target_Far, Ammo_Preferable), Desirable);
  m_FuzzyModule.AddRule(FzAND(Target_Far, Ammo_Loads), Desirable);
  
}


//-------------------------------- Render -------------------------------------
//-----------------------------------------------------------------------------
void RocketLauncher::Render()
{
    m_vecWeaponVBTrans = WorldTransform(m_vecWeaponVB,
                                   m_pOwner->Pos(),
                                   m_pOwner->Facing(),
                                   m_pOwner->Facing().Perp(),
                                   m_pOwner->Scale());

  gdi->RedPen();

  gdi->ClosedShape(m_vecWeaponVBTrans);
}