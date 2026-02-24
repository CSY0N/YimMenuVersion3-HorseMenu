#include "core/scripting/LuaLibrary.hpp"
#include "core/scripting/LuaMenuResource.hpp"
#include "core/scripting/LuaScript.hpp"
#include "core/scripting/LuaUtils.hpp"
#include "core/frontend/manager/Submenu.hpp"
#include "core/frontend/manager/UIManager.hpp"
#include "game/frontend/items/Items.hpp"

#include <atomic>
#include <queue>
#include <variant>

namespace YimMenu::Lua
{
	// Stored as userdata so Lua scripts can hold references to submenus and categories
	struct LuaSubmenuRef
	{
		std::shared_ptr<Submenu> m_Submenu;
	};

	struct LuaCategoryRef
	{
		std::shared_ptr<Category> m_Category;
	};

	// Pending callback event queued from the DX thread, dispatched during script tick
	struct PendingMenuCallback
	{
		int m_FuncRef;
		std::vector<std::variant<bool, int, float, std::string>> m_Args;
	};

	// Per-script queue for pending menu callbacks (DX thread -> script thread)
	struct LuaMenuCallbackQueue
	{
		std::mutex m_Mutex;
		std::queue<PendingMenuCallback> m_Queue;

		void Push(PendingMenuCallback&& cb)
		{
			std::lock_guard lock(m_Mutex);
			m_Queue.push(std::move(cb));
		}

		bool Pop(PendingMenuCallback& out)
		{
			std::lock_guard lock(m_Mutex);
			if (m_Queue.empty())
				return false;
			out = std::move(m_Queue.front());
			m_Queue.pop();
			return true;
		}
	};

	// Helper to push a variant arg onto the Lua stack
	static void PushArg(lua_State* state, const std::variant<bool, int, float, std::string>& arg)
	{
		std::visit([state](auto&& val) {
			using T = std::decay_t<decltype(val)>;
			if constexpr (std::is_same_v<T, bool>)
				lua_pushboolean(state, val);
			else if constexpr (std::is_same_v<T, int>)
				lua_pushinteger(state, val);
			else if constexpr (std::is_same_v<T, float>)
				lua_pushnumber(state, val);
			else if constexpr (std::is_same_v<T, std::string>)
				lua_pushstring(state, val.c_str());
		}, arg);
	}

	class MenuLib : LuaLibrary
	{
	public:
		using LuaLibrary::LuaLibrary;

		// Retrieves or creates the per-script callback queue stored in the Lua registry
		static std::shared_ptr<LuaMenuCallbackQueue> GetCallbackQueue(lua_State* state)
		{
			lua_getfield(state, LUA_REGISTRYINDEX, "_menu_callback_queue");
			if (lua_isnil(state, -1))
			{
				lua_pop(state, 1);
				auto queue = std::make_shared<LuaMenuCallbackQueue>();
				auto** ud = static_cast<std::shared_ptr<LuaMenuCallbackQueue>**>(lua_newuserdata(state, sizeof(std::shared_ptr<LuaMenuCallbackQueue>*)));
				*ud = new std::shared_ptr<LuaMenuCallbackQueue>(queue);
				lua_setfield(state, LUA_REGISTRYINDEX, "_menu_callback_queue");
				return queue;
			}
			auto** ud = static_cast<std::shared_ptr<LuaMenuCallbackQueue>**>(lua_touserdata(state, -1));
			lua_pop(state, 1);
			return **ud;
		}

		// menu.add_submenu("Name", "icon")
		static int AddSubmenu(lua_State* state)
		{
			auto& script = LuaScript::GetScript(state);
			const char* name = CheckStringSafe(state, 1);
			const char* icon = lua_gettop(state) >= 2 ? CheckStringSafe(state, 2) : "";

			auto submenu = std::make_shared<Submenu>(name, icon);

			auto resource = std::make_shared<LuaSubmenuResource>(submenu);
			script.AddResource(std::move(resource), g_LuaMenuResourceType.GetIndex());

			UIManager::AddSubmenu(std::shared_ptr<Submenu>(submenu));

			auto* ref = static_cast<LuaSubmenuRef*>(lua_newuserdata(state, sizeof(LuaSubmenuRef)));
			new (ref) LuaSubmenuRef{submenu};
			luaL_getmetatable(state, "SubmenuRef");
			lua_setmetatable(state, -2);

			return 1;
		}

		// menu.find_submenu("Name")
		static int FindSubmenu(lua_State* state)
		{
			const char* name = CheckStringSafe(state, 1);
			auto submenu = UIManager::FindSubmenuByName(name);
			if (!submenu)
			{
				lua_pushnil(state);
				return 1;
			}

			auto* ref = static_cast<LuaSubmenuRef*>(lua_newuserdata(state, sizeof(LuaSubmenuRef)));
			new (ref) LuaSubmenuRef{submenu};
			luaL_getmetatable(state, "SubmenuRef");
			lua_setmetatable(state, -2);

			return 1;
		}

		// menu.process_callbacks() - must be called from a script callback loop to dispatch queued UI events
		static int ProcessCallbacks(lua_State* state)
		{
			auto queue = GetCallbackQueue(state);
			PendingMenuCallback cb;
			int count = 0;
			while (queue->Pop(cb))
			{
				lua_rawgeti(state, LUA_REGISTRYINDEX, cb.m_FuncRef);
				for (auto& arg : cb.m_Args)
					PushArg(state, arg);

				if (lua_pcall(state, (int)cb.m_Args.size(), 0, 0) != LUA_OK)
				{
					auto err = lua_tostring(state, -1);
					LOGF(FATAL, "{}: menu callback error: {}", LuaScript::GetScript(state).GetName(), err);
					lua_pop(state, 1);
				}
				count++;
			}
			lua_pushinteger(state, count);
			return 1;
		}

		// submenu:add_category("CategoryName")
		static int SubmenuAddCategory(lua_State* state)
		{
			auto* ref = static_cast<LuaSubmenuRef*>(luaL_checkudata(state, 1, "SubmenuRef"));
			const char* name = CheckStringSafe(state, 2);

			auto category = std::make_shared<Category>(name);
			ref->m_Submenu->AddCategory(std::shared_ptr<Category>(category));

			auto* cat_ref = static_cast<LuaCategoryRef*>(lua_newuserdata(state, sizeof(LuaCategoryRef)));
			new (cat_ref) LuaCategoryRef{category};
			luaL_getmetatable(state, "CategoryRef");
			lua_setmetatable(state, -2);

			return 1;
		}

		// submenu:get_name()
		static int SubmenuGetName(lua_State* state)
		{
			auto* ref = static_cast<LuaSubmenuRef*>(luaL_checkudata(state, 1, "SubmenuRef"));
			lua_pushstring(state, ref->m_Submenu->m_Name.c_str());
			return 1;
		}

		// category:add_button("Label", callback)
		static int CategoryAddButton(lua_State* state)
		{
			auto* ref = static_cast<LuaCategoryRef*>(luaL_checkudata(state, 1, "CategoryRef"));
			const char* label = CheckStringSafe(state, 2);
			luaL_checktype(state, 3, LUA_TFUNCTION);

			lua_pushvalue(state, 3);
			int func_ref = luaL_ref(state, LUA_REGISTRYINDEX);

			std::string label_str(label);
			auto queue = GetCallbackQueue(state);

			ref->m_Category->AddItem(std::make_shared<ImGuiItem>([label_str, func_ref, queue]() {
				if (ImGui::Button(label_str.c_str()))
				{
					queue->Push({func_ref, {}});
				}
			}));

			return 0;
		}

		// category:add_checkbox("Label", default_value, on_change_callback)
		static int CategoryAddCheckbox(lua_State* state)
		{
			auto* ref = static_cast<LuaCategoryRef*>(luaL_checkudata(state, 1, "CategoryRef"));
			const char* label = CheckStringSafe(state, 2);
			bool default_val = lua_gettop(state) >= 3 && lua_type(state, 3) == LUA_TBOOLEAN ? lua_toboolean(state, 3) : false;

			int func_ref = LUA_NOREF;
			std::shared_ptr<LuaMenuCallbackQueue> queue;
			if (lua_gettop(state) >= 4 && lua_type(state, 4) == LUA_TFUNCTION)
			{
				lua_pushvalue(state, 4);
				func_ref = luaL_ref(state, LUA_REGISTRYINDEX);
				queue = GetCallbackQueue(state);
			}

			std::string label_str(label);
			auto checked = std::make_shared<bool>(default_val);

			ref->m_Category->AddItem(std::make_shared<ImGuiItem>([label_str, checked, func_ref, queue]() {
				if (ImGui::Checkbox(label_str.c_str(), checked.get()))
				{
					if (func_ref != LUA_NOREF && queue)
					{
						queue->Push({func_ref, {*checked}});
					}
				}
			}));

			return 0;
		}

		// category:add_int_slider("Label", min, max, default, on_change_callback)
		static int CategoryAddIntSlider(lua_State* state)
		{
			auto* ref = static_cast<LuaCategoryRef*>(luaL_checkudata(state, 1, "CategoryRef"));
			const char* label = CheckStringSafe(state, 2);
			int min_val = luaL_checkinteger(state, 3);
			int max_val = luaL_checkinteger(state, 4);
			int default_val = lua_gettop(state) >= 5 ? luaL_checkinteger(state, 5) : min_val;

			int func_ref = LUA_NOREF;
			std::shared_ptr<LuaMenuCallbackQueue> queue;
			if (lua_gettop(state) >= 6 && lua_type(state, 6) == LUA_TFUNCTION)
			{
				lua_pushvalue(state, 6);
				func_ref = luaL_ref(state, LUA_REGISTRYINDEX);
				queue = GetCallbackQueue(state);
			}

			std::string label_str(label);
			auto value = std::make_shared<int>(default_val);

			ref->m_Category->AddItem(std::make_shared<ImGuiItem>([label_str, value, min_val, max_val, func_ref, queue]() {
				if (ImGui::SliderInt(label_str.c_str(), value.get(), min_val, max_val))
				{
					if (func_ref != LUA_NOREF && queue)
					{
						queue->Push({func_ref, {*value}});
					}
				}
			}));

			return 0;
		}

		// category:add_float_slider("Label", min, max, default, on_change_callback)
		static int CategoryAddFloatSlider(lua_State* state)
		{
			auto* ref = static_cast<LuaCategoryRef*>(luaL_checkudata(state, 1, "CategoryRef"));
			const char* label = CheckStringSafe(state, 2);
			float min_val = luaL_checknumber(state, 3);
			float max_val = luaL_checknumber(state, 4);
			float default_val = lua_gettop(state) >= 5 ? luaL_checknumber(state, 5) : min_val;

			int func_ref = LUA_NOREF;
			std::shared_ptr<LuaMenuCallbackQueue> queue;
			if (lua_gettop(state) >= 6 && lua_type(state, 6) == LUA_TFUNCTION)
			{
				lua_pushvalue(state, 6);
				func_ref = luaL_ref(state, LUA_REGISTRYINDEX);
				queue = GetCallbackQueue(state);
			}

			std::string label_str(label);
			auto value = std::make_shared<float>(default_val);

			ref->m_Category->AddItem(std::make_shared<ImGuiItem>([label_str, value, min_val, max_val, func_ref, queue]() {
				if (ImGui::SliderFloat(label_str.c_str(), value.get(), min_val, max_val))
				{
					if (func_ref != LUA_NOREF && queue)
					{
						queue->Push({func_ref, {*value}});
					}
				}
			}));

			return 0;
		}

		// category:add_input_text("Label", "hint", on_enter_callback)
		static int CategoryAddInputText(lua_State* state)
		{
			auto* ref = static_cast<LuaCategoryRef*>(luaL_checkudata(state, 1, "CategoryRef"));
			const char* label = CheckStringSafe(state, 2);
			const char* hint = lua_gettop(state) >= 3 && lua_type(state, 3) == LUA_TSTRING ? lua_tostring(state, 3) : "";

			int func_ref = LUA_NOREF;
			std::shared_ptr<LuaMenuCallbackQueue> queue;
			if (lua_gettop(state) >= 4 && lua_type(state, 4) == LUA_TFUNCTION)
			{
				lua_pushvalue(state, 4);
				func_ref = luaL_ref(state, LUA_REGISTRYINDEX);
				queue = GetCallbackQueue(state);
			}

			std::string label_str(label);
			std::string hint_str(hint);
			auto buf = std::make_shared<std::array<char, 256>>();
			buf->fill(0);

			ref->m_Category->AddItem(std::make_shared<ImGuiItem>([label_str, hint_str, buf, func_ref, queue]() {
				if (ImGui::InputTextWithHint(label_str.c_str(), hint_str.c_str(), buf->data(), buf->size(), ImGuiInputTextFlags_EnterReturnsTrue))
				{
					if (func_ref != LUA_NOREF && queue)
					{
						queue->Push({func_ref, {std::string(buf->data())}});
					}
				}
			}));

			return 0;
		}

		// category:add_separator()
		static int CategoryAddSeparator(lua_State* state)
		{
			auto* ref = static_cast<LuaCategoryRef*>(luaL_checkudata(state, 1, "CategoryRef"));

			ref->m_Category->AddItem(std::make_shared<ImGuiItem>([]() {
				ImGui::Separator();
			}));

			return 0;
		}

		// category:add_text("Some text")
		static int CategoryAddText(lua_State* state)
		{
			auto* ref = static_cast<LuaCategoryRef*>(luaL_checkudata(state, 1, "CategoryRef"));
			const char* text = CheckStringSafe(state, 2);

			std::string text_str(text);
			ref->m_Category->AddItem(std::make_shared<ImGuiItem>([text_str]() {
				ImGui::Text("%s", text_str.c_str());
			}));

			return 0;
		}

		// category:get_name()
		static int CategoryGetName(lua_State* state)
		{
			auto* ref = static_cast<LuaCategoryRef*>(luaL_checkudata(state, 1, "CategoryRef"));
			lua_pushstring(state, ref->m_Category->m_Name.c_str());
			return 1;
		}

		static int SubmenuRefGC(lua_State* state)
		{
			auto* ref = static_cast<LuaSubmenuRef*>(lua_touserdata(state, 1));
			ref->~LuaSubmenuRef();
			return 0;
		}

		static int CategoryRefGC(lua_State* state)
		{
			auto* ref = static_cast<LuaCategoryRef*>(lua_touserdata(state, 1));
			ref->~LuaCategoryRef();
			return 0;
		}

		virtual void Register(lua_State* state) override
		{
			// SubmenuRef metatable
			luaL_newmetatable(state, "SubmenuRef");
			{
				lua_newtable(state);
				SetFunction(state, SubmenuAddCategory, "add_category");
				SetFunction(state, SubmenuGetName, "get_name");
				lua_setfield(state, -2, "__index");

				SetFunction(state, SubmenuRefGC, "__gc");
			}
			lua_pop(state, 1);

			// CategoryRef metatable
			luaL_newmetatable(state, "CategoryRef");
			{
				lua_newtable(state);
				SetFunction(state, CategoryAddButton, "add_button");
				SetFunction(state, CategoryAddCheckbox, "add_checkbox");
				SetFunction(state, CategoryAddIntSlider, "add_int_slider");
				SetFunction(state, CategoryAddFloatSlider, "add_float_slider");
				SetFunction(state, CategoryAddInputText, "add_input_text");
				SetFunction(state, CategoryAddSeparator, "add_separator");
				SetFunction(state, CategoryAddText, "add_text");
				SetFunction(state, CategoryGetName, "get_name");
				lua_setfield(state, -2, "__index");

				SetFunction(state, CategoryRefGC, "__gc");
			}
			lua_pop(state, 1);

			// Global "menu" table
			lua_newtable(state);
			SetFunction(state, AddSubmenu, "add_submenu");
			SetFunction(state, FindSubmenu, "find_submenu");
			SetFunction(state, ProcessCallbacks, "process_callbacks");
			lua_setglobal(state, "menu");
		}
	};

	MenuLib _MenuLib;
}