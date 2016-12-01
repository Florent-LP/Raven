#include "Goal_DodgeFollowingPath.h"
#include "../Raven_Bot.h"
#include "../Raven_Game.h"

#include "Goal_TraverseEdge.h"
#include "Goal_NegotiateDoor.h"
#include "misc/cgdi.h"
#include "Goal_DodgeSideToSide.h"


//----------------------------- Construct -------------------------------------
//-----------------------------------------------------------------------------
Goal_DodgeFollowingPath::Goal_DodgeFollowingPath(Raven_Bot* pBot, std::list<PathEdge> path) :
	Goal_Composite<Raven_Bot>(pBot, goal_dodge_following_path),
	m_Path(path)
{}

//------------------------------ Activate -------------------------------------
//-----------------------------------------------------------------------------
void Goal_DodgeFollowingPath::Activate() {
	m_iStatus = active;

	PathEdge edge = m_Path.front();
	m_Path.pop_front();

	Vector2D dummy;
	switch (edge.Behavior()) {

		case NavGraphEdge::normal:
			if (m_pOwner->GetTargetBot() != nullptr) {
				Vector2D ToTarget = m_pOwner->GetTargetBot()->Pos() - m_pOwner->Pos();

				if (ToTarget.Length() > 25 && ( m_pOwner->canStepLeft(dummy) || m_pOwner->canStepRight(dummy) ))
					AddSubgoal(new Goal_DodgeSideToSide(m_pOwner));
			}
			AddSubgoal(new Goal_TraverseEdge(m_pOwner, edge, m_Path.empty()));
			break;

		case NavGraphEdge::goes_through_door:
			AddSubgoal(new Goal_NegotiateDoor(m_pOwner, edge, m_Path.empty()));
			break;

		default:
			throw std::runtime_error("<Goal_DodgeFollowingPath::Activate>: Unrecognized edge type");

	}
}

//--------------------------------- Process -----------------------------------
//-----------------------------------------------------------------------------
int Goal_DodgeFollowingPath::Process() {
	ActivateIfInactive();

	m_iStatus = ProcessSubgoals();
	if (m_iStatus == completed && !m_Path.empty())
		Activate();

	return m_iStatus;
}

//--------------------------------- Render ------------------------------------
//-----------------------------------------------------------------------------
void Goal_DodgeFollowingPath::Render()
{
	//render all the path waypoints remaining on the path list
	std::list<PathEdge>::iterator it;
	for (it = m_Path.begin(); it != m_Path.end(); ++it)
	{
		gdi->BlackPen();
		gdi->LineWithArrow(it->Source(), it->Destination(), 5);

		gdi->RedBrush();
		gdi->BlackPen();
		gdi->Circle(it->Destination(), 3);
	}

	//forward the request to the subgoals
	Goal_Composite<Raven_Bot>::Render();
}
