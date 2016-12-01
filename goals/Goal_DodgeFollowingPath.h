#pragma once
#pragma warning (disable:4786)

#include "Goals/Goal_Composite.h"
#include "Raven_Goal_Types.h"
#include "../Raven_Bot.h"
#include "../navigation/Raven_PathPlanner.h"
#include "../navigation/PathEdge.h"

class Goal_DodgeFollowingPath : public Goal_Composite<Raven_Bot> {
	private:
		//a local copy of the path returned by the path planner
		std::list<PathEdge>  m_Path;

	public:
		Goal_DodgeFollowingPath(Raven_Bot* pBot, std::list<PathEdge> path);

		//the usual suspects
		void Activate();
		int Process();
		void Render();
		void Terminate() {}
};