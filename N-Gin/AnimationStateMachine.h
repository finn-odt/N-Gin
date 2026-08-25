#pragma once

#include <stack>
#include "AnimationState.h"

class AnimationStateMachine
{
	std::stack<AnimationState> stateStack;
	std::stack<AnimationState> undoStateStack;  // could be useful in the future?

	public:
		AnimationStateMachine() = default;
		~AnimationStateMachine() = default;

		void ChangeState(AnimationState nextState)
		{
			stateStack.push(nextState);  // push new state
		}

		AnimationState BackToPreviousState()
		{
			undoStateStack.push(stateStack.top());  // save current state in undo-stack
			stateStack.pop();  // remove current
			return stateStack.top();  // return new current state
		}

		AnimationState GetCurrentState()
		{
			if (stateStack.empty())
				throw std::logic_error("No current animation state.");

			return stateStack.top();
		}
};
