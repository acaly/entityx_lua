#include "entityx_lua.h"

using namespace entityx::lua;
ComponentList entityx::lua::components;

namespace {
	int ref_EntityManagerSharedPtr = LUA_REFNIL;
	int ref_EntityId = LUA_REFNIL;
	const char* TypeName_EntityManagerSharedPtr = "EntityManagerSharedPtr";
	const char* TypeName_EntityId = "EntityId";
	const char* setup_lua_create_entity_factory =
		"function entityx.create_entity_factory(entity_manager_pointer)\n"
		"	local c = {}\n"
		"	c.__index = c\n"
		"	function c:new()\n"
		"		local ret = {}\n"
		"		ret.__index = ret\n"
		"		function ret:init()\n"
		"			-- Empty\n"
		"		end\n"
		"		function ret:instance(param)\n"
		"			-- Make id const\n"
		"			local r = {}\n"
		"			local id = entityx.create_new_entity(entity_manager_pointer, r)\n"
		"			r.id = id\n"
		"			function r:component(name, params)\n"
		"				return entityx.create_new_component(name, entity_manager_pointer, id, params)\n"
		"			end\n"
		"			setmetatable(r, self)\n"
		"			r:init(param)\n"
		"			return r\n"
		"		end\n"
		"		return ret\n"
		"	end\n"
		"	function c:get(id)\n"
		"		return entityx.get_lua_object(entity_manager_pointer, id)\n"
		"	end\n"
		"	return c\n"
		"end\n";
}

//TODO replace:entityx_lua:: settable->setfield/rawset
void entityx::lua::setup_entityx_api(lua_State* L)
{
	// Create metatables for userdata
	// metatable for shared_ptr of EntityManager
	lua_newtable(L);
	lua_pushstring(L, TypeName_EntityManagerSharedPtr);
	lua_setfield(L, -2, "type");
	lua_pushcfunction(L, [](lua_State* L){
		EntityManagerSharedPtr* ptr = static_cast<EntityManagerSharedPtr*>(lua_touserdata(L, 1));
		(*ptr).~EntityManagerSharedPtr();
		return 0;
	});
	lua_setfield(L, -2, "__gc");
	ref_EntityManagerSharedPtr = luaL_ref(L, LUA_REGISTRYINDEX);

	// metatable for Entity::Id
	lua_newtable(L);
	lua_pushstring(L, TypeName_EntityId);
	lua_setfield(L, -2, "type");
	// We don't need __gc for Entity::Id (with static_assert it is_trivially_destructible)
	ref_EntityId = luaL_ref(L, LUA_REGISTRYINDEX);


	// Create global entityx table
	lua_newtable(L);

	// create_new_entity
	lua_pushcfunction(L, [](lua_State* L){
		// First check the type of the parameter
		lua_getmetatable(L, 1);
		lua_pushstring(L, "type");
		lua_rawget(L, -2);
		if (!lua_isuserdata(L, 1) || strcmp(lua_tostring(L, -1), TypeName_EntityManagerSharedPtr) != 0 || !lua_istable(L, 2))
		{
			lua_pushstring(L, "create_new_entity should be called with pointer of EntityManager and a table.");
			return lua_error(L);
		}
		lua_pop(L, 2);

		lua_pushvalue(L, 2);
		int ref = luaL_ref(L, LUA_REGISTRYINDEX);

		// Create an new entity
		EntityManagerSharedPtr* ptr = static_cast<EntityManagerSharedPtr*>(lua_touserdata(L, 1));
		entityx::Entity entity = (*ptr)->create();

		// Save the lua object (ref) in the entity
		entity.assign<LuaComponent>(ref);

		// Return the Id of the entity
		lua_newuserdata(L, sizeof(entityx::Entity::Id));
		lua_rawgeti(L, LUA_REGISTRYINDEX, ref_EntityId);
		lua_setmetatable(L, -2);
		new (lua_touserdata(L, -1)) entityx::Entity::Id(entity.id());

		return 1;
	});
	lua_setfield(L, -2, "create_new_entity");

	// get_lua_object
	lua_pushcfunction(L, [](lua_State* L){
		// First check the type of the parameters
		lua_getmetatable(L, 1);
		lua_pushstring(L, "type");
		lua_rawget(L, -2);
		if (!lua_isuserdata(L, 1) || strcmp(lua_tostring(L, -1), TypeName_EntityManagerSharedPtr) != 0)
		{
			lua_pushstring(L, "get_lua_object should be called with pointer of EntityManager and an EntityId.");
			return lua_error(L);
		}
		lua_pop(L, 2);

		lua_getmetatable(L, 2);
		lua_pushstring(L, "type");
		lua_rawget(L, -2);
		if (!lua_isuserdata(L, 2) || strcmp(lua_tostring(L, -1), TypeName_EntityId) != 0)
		{
			lua_pushstring(L, "get_lua_object should be called with pointer of EntityManager and an EntityId.");
			return lua_error(L);
		}
		lua_pop(L, 2);

		// Get the object
		EntityManagerSharedPtr* ptr = static_cast<EntityManagerSharedPtr*>(lua_touserdata(L, 1));
		entityx::Entity::Id* id = static_cast<entityx::Entity::Id*>(lua_touserdata(L, 2));

		entityx::Entity entity = (*ptr)->get(*id);
		int ref = entity.component<LuaComponent>()->ref;

		lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
		return 1;
	});
	lua_setfield(L, -2, "get_lua_object");

	// create_new_component
	lua_pushcfunction(L, [](lua_State* L){
		ComponentList::iterator i;
		//return 0;
		if ((i = components.find(std::string(lua_tostring(L, 1)))) != components.end())
		{
			i->second(L);
		}
		else
		{
			lua_pushstring(L, "Component not found.");
			return lua_error(L);
		}
		return 1;
	});
	lua_setfield(L, -2, "create_new_component");

	lua_setglobal(L, "entityx");

	luaL_loadstring(L, setup_lua_create_entity_factory);
	lua_call(L, 0, 0);
}

void entityx::lua::push_entity_manager(lua_State* L, const EntityManagerSharedPtr& manager)
{
	lua_newuserdata(L, sizeof(EntityManagerSharedPtr));
	lua_rawgeti(L, LUA_REGISTRYINDEX, ref_EntityManagerSharedPtr);
	lua_setmetatable(L, -2);

	new (lua_touserdata(L, -1)) EntityManagerSharedPtr(manager);
}

void entityx::lua::new_entity_manager(lua_State* L, const EntityManagerSharedPtr& manager, const char* name)
{
	lua_getglobal(L, "entityx");
	lua_getfield(L, -1, "create_entity_factory");
	lua_remove(L, -2); // _G["entityx"]
	push_entity_manager(L, manager);
	lua_pcall(L, 1, 1, 1);

	lua_getglobal(L, "entityx");
	lua_pushvalue(L, -2);
	lua_setfield(L, -2, name);
	lua_pop(L, 2); // original return value & entityx
}