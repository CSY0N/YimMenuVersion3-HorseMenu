#include "Entity.hpp"
#include "core/scripting/LatentFunction.hpp"
#include "core/scripting/LuaLibrary.hpp"
#include "core/scripting/LuaScript.hpp"
#include "core/scripting/LuaUtils.hpp"
#include "game/rdr/Ped.hpp"

namespace YimMenu::Lua
{
	class Ped : LuaLibrary
	{
	public:
		using LuaLibrary::LuaLibrary;

		static int New(lua_State* state)
		{
			CreateObject<YimMenu::Ped>(state, luaL_checkinteger(state, 1));
			return 1;
		}

		static int Create(lua_State* state)
		{
			CopyObject<YimMenu::Ped>(state, YimMenu::Ped::Create(GetHashArgument(state, 1), GetObject<rage::fvector3>(state, 2), lua_gettop(state) >= 3 ? luaL_checknumber(state, 3) : 0.0f));
			return 1;
		}

		static int GetVehicle(lua_State* state)
		{
			CopyObject<YimMenu::Vehicle>(state, GetObject<YimMenu::Ped>(state, 1).GetVehicle());
			return 1;
		}

		static int GetRagdoll(lua_State* state)
		{
			lua_pushboolean(state, GetObject<YimMenu::Ped>(state, 1).GetRagdoll());
			return 1;
		}

		static int SetRagdoll(lua_State* state)
		{
			GetObject<YimMenu::Ped>(state, 1).SetRagdoll(CheckBooleanSafe(state, 2));
			return 0;
		}

		static int GetBonePosition(lua_State* state)
		{
			MoveObject<rage::fvector3>(state, GetObject<YimMenu::Ped>(state, 1).GetBonePosition(luaL_checkinteger(state, 2)));
			return 0;
		}

		// GetConfigFlag
		// SetConfigFlag
		// SetCombatAttribute

		static int IsEnemy(lua_State* state)
		{
			lua_pushboolean(state, GetObject<YimMenu::Ped>(state, 1).IsEnemy());
			return 1;
		}

		static int GetAccuracy(lua_State* state)
		{
			lua_pushinteger(state, GetObject<YimMenu::Ped>(state, 1).GetAccuracy());
			return 1;
		}

		static int SetAccuracy(lua_State* state)
		{
			GetObject<YimMenu::Ped>(state, 1).SetAccuracy(luaL_checkinteger(state, 2));
			return 0;
		}

		virtual void Register(lua_State* state) override
		{
			luaL_newmetatable(state, "Ped");
			{
				lua_newtable(state);
				{
					RegisterEntityMethods(state); // re-registering the entity methods under the vehicle table is more efficient than subclassing
					SetFunction(state, GetVehicle, "get_vehicle");
					SetFunction(state, GetRagdoll, "get_ragdoll");
					SetFunction(state, SetRagdoll, "set_ragdoll");
					SetFunction(state, GetBonePosition, "get_bone_position");
					SetFunction(state, IsEnemy, "is_enemy");
					SetFunction(state, GetAccuracy, "get_accuracy");
					SetFunction(state, SetAccuracy, "set_accuracy");
				}
				lua_setfield(state, -2, "__index"); // prototype
			}
			Metatable<YimMenu::Ped>::Register(state);
			Metatable<YimMenu::Entity>::AddSubclass<YimMenu::Ped>();

			lua_newtable(state);
			SetConstructor<New>(state);
			SetFunction(state, LatentFunction<Create>, "create");
			lua_setglobal(state, "Ped");
		}
	};

	Ped _Ped;
}