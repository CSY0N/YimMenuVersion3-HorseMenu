#include "LuaMenuResource.hpp"
#include "core/frontend/manager/UIManager.hpp"

namespace YimMenu
{
	LuaMenuResourceType g_LuaMenuResourceType;

	void LuaSubmenuResource::OnEnable()
	{
		UIManager::AddSubmenu(std::shared_ptr<Submenu>(m_Submenu));
	}

	void LuaSubmenuResource::OnDisable()
	{
		UIManager::RemoveSubmenu(m_Submenu);
	}
}