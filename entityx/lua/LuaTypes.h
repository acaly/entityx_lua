#pragma once

namespace entityx { namespace lua
{
	// double
	template <>
	inline void LuaToC<double>(lua_State* L, double& ref)
	{
		ref = lua_tonumber(L, -1);
	}
	template <>
	inline void CToLua<double>(lua_State* L, double& ref)
	{
		lua_pushnumber(L, ref);
	}

	// float
	template <>
	inline void LuaToC<float>(lua_State* L, float& ref)
	{
		ref = static_cast<float>(lua_tonumber(L, -1));
	}
	template <>
	inline void CToLua<float>(lua_State* L, float& ref)
	{
		lua_pushnumber(L, ref);
	}

	// int
	template <>
	inline void LuaToC<int>(lua_State* L, int& ref)
	{
		ref = lua_tointeger(L, -1);
	}
	template <>
	inline void CToLua<int>(lua_State* L, int& ref)
	{
		lua_pushinteger(L, ref);
	}
}}
