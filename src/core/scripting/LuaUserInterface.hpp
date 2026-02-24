#pragma once
#include <unordered_set>


namespace YimMenu
{
	class LuaUserInterface;
}
namespace YimMenu::Lua
{
	class UIRoot;

	class UIElementBase
	{
		LuaUserInterface& m_Interface;

	public:
		UIElementBase(LuaUserInterface& _interface);
		virtual void Draw() = 0;
	};

	class UIElementGroup : public UIElementBase
	{
		std::string m_Name;
		int m_NumElems; // num elements per row
		                //std::vector<std::unique_ptr<UIElement>> m_Elements;

	public:
		UIElementGroup(LuaUserInterface& _interface, std::string_view name, int num_elems = 7);
		virtual void Draw();
	};

	class UIElement : public UIElementBase
	{
		std::string m_CmdString;
		std::uint32_t m_CmdHash;
		std::string m_Label;

	public:
		virtual void Draw() override = 0;
	};

	class UIElementWithState : public UIElement
	{
		bool m_ShouldSerialize;

	public:
		virtual void Draw() = 0;
	};

	class UIElementButton : public UIElement
	{
		int m_Function;

	public:
		virtual void Draw() = 0;
	};

	class UIButtonCheckbox : public UIElementWithState
	{
		bool m_Value;
		int m_OnChange;

	public:
		virtual void Draw() = 0;
	};

	class UIButtonIntSlider : public UIElementWithState
	{
		int m_Value;
		int m_OnChange;

	public:
		virtual void Draw() = 0;
	};

	class UIRoot
	{
	};
}

namespace YimMenu
{
	class LuaUserInterface
	{
		std::mutex m_TickFunctionsLock;
		std::unordered_set<int> m_TickFunctions;
		std::deque<int> m_ThrottledCoroutines;
		std::chrono::system_clock::time_point m_LastThrotlledCoroutinePush;
		std::unordered_map<std::uint32_t, Lua::UIElement*> m_ElementsById;

	public:
		void AddTickFunction(int func);
		void RemoveTickFunction(int func);
		void QueueCoroutine(int coro, bool immediate = false);

		// must be called from the main thread
		void Tick();

		// must be called from the DX thread
		void Draw();
	};
}