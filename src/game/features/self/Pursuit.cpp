#include "core/commands/Command.hpp"
#include "game/backend/Self.hpp"
#include "game/rdr/Natives.hpp"
namespace YimMenu::Features
{
	class Pursuit : public Command
	{
		using Command::Command;
		virtual void OnCall() override
		{
			LAW::_SET_BOUNTY_HUNTER_PURSUIT_CLEARED();
		}
	};
	static Pursuit _Pursuit{"endPursuit", "End Pursuit", "End current police pursuit"};
}
