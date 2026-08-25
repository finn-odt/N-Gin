#pragma once

class AnimationState
{

	public:
		enum StateType
		{
			INTRO,
			PART1,
			PART2
		};

		StateType type;

		AnimationState(const StateType type)
		{
			this->type = type;
		}
};
