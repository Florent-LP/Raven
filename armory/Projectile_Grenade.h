#pragma once
#pragma warning (disable:4786)

#include "Raven_Projectile.h"
class Raven_Bot;

class Grenade : public Raven_Projectile {
	private:
		double m_dBlastRadius;
		double m_dCurrentBlastRadius;
		double m_timeBeforeBlast;

		void InflictDamageOnBotsWithinBlastRadius();
		void TestForImpact();

	public:
		Grenade(Raven_Bot* shooter, Vector2D target);

		void Render();
		void Update();
};
