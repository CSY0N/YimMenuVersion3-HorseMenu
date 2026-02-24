#pragma once
#include "LuaResource.hpp"
#include "core/frontend/manager/Submenu.hpp"
#include "core/frontend/manager/Category.hpp"
#include "core/frontend/manager/UIItem.hpp"

namespace YimMenu
{
	class LuaMenuResourceType : public LuaResourceType
	{
	};

	class LuaSubmenuResource : public LuaResource
	{
		std::shared_ptr<Submenu> m_Submenu;

	public:
		LuaSubmenuResource(std::shared_ptr<Submenu> submenu) :
		    m_Submenu(std::move(submenu))
		{
		}

		std::shared_ptr<Submenu>& GetSubmenu()
		{
			return m_Submenu;
		}

		void OnEnable() override;
		void OnDisable() override;
	};

	extern LuaMenuResourceType g_LuaMenuResourceType;
}