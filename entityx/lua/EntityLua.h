#pragma once

// Check is_trivially_destructible so we don't need __gc for userdata.
static_assert(std::is_trivially_destructible<entityx::Entity::Id>::value, "Entity::Id is not trivially destructible");
// Check is_copy_constructible so that we can copy data onto memory for userdata created by lua.
static_assert(std::is_copy_constructible<entityx::Entity::Id>::value, "Entity::Id is not copy constructible");

namespace entityx { namespace lua
{
	typedef entityx::ptr<entityx::EntityManager> EntityManagerSharedPtr;

	// This component is internally assigned to all entities created from lua.
	// It is used to store the ref of the lua table
	struct LuaComponent : entityx::Component<LuaComponent>
	{
		// ref of the lua object in LUA_REGISTRYINDEX
		int ref;
		LuaComponent(int ref = LUA_REFNIL) : ref(ref) {}
	};

	// This function should be called right after luaL_openlibs, to create global entityx table
	void setup_entityx_api(lua_State* L);

	// This function add a new entity manager to lua global "entityx.xxx" where xxx is the name provided
	void new_entity_manager(lua_State* L, const EntityManagerSharedPtr& manager, const char* name);

	// This function is used to push an entity manager to lua.
	// It only pushes the userdata onto the stack, so use lua api such as lua_setglobal,
	// lua_setfield, or lua_settable to really save it to lua.
	void push_entity_manager(lua_State* L, const EntityManagerSharedPtr& manager);

	typedef std::map<std::string, std::function<void(lua_State*)>> ComponentList;
	extern ComponentList components;

	template <typename C>
	class MemberRegister
	{
		lua_State* L;
	public:
		MemberRegister(lua_State* L) : L(L) {}

		template <typename T>
		void add(const char* name, T C::*ptr)
		{
			// Now the ComponentHandler should be at #-2 of stack (-1 is the metatable), this will be used as up-values
			lua_pushvalue(L, -2);
			// Also remember the offset
			int offset = ComponentHelper<C>::offset(ptr);
			lua_pushinteger(L, offset);
			lua_pushcclosure(L, [](lua_State* L){
				// This function is used as both getter and setter:
				// value = entity.comp.x()
				// entity.comp.x(value)
				// Note that self is not needed (that is, use comp.x() instead of comp:x())
				ComponentHelper<C>::ptr* pptr = static_cast<ComponentHelper<C>::ptr*>(lua_touserdata(L, lua_upvalueindex(1)));
				int offset = lua_tointeger(L, lua_upvalueindex(2));

				T& ref = ComponentHelper<C>::get<T>(**pptr, offset);
				if (lua_gettop(L) == 0 || lua_isnil(L, 1))
				{
					// Getter mode, push value of at pptr
					CToLua(L, ref);
				}
				else
				{
					// Setter mode, set value to pptr
					LuaToC(L, ref);
				}
				return 1;
			}, 2);
			lua_setfield(L, -2, name);
		}
	};

	template <typename C>
	struct ComponentHelper
	{
		// Use this ptr to get the ComponentHandle type, to support both 0.x and 1.x of entityx.
		typedef decltype(entityx::Entity().assign<C>()) ptr;

		template <typename Member>
		static ptrdiff_t offset(Member ptr)
		{
			C c;
			return static_cast<char*>(static_cast<void*>(&(c.*ptr))) - static_cast<char*>(static_cast<void*>(&c));
		}

		template <typename T>
		static T& get(C& c, ptrdiff_t offset)
		{
			return *static_cast<T*>(static_cast<void*>(
				static_cast<char*>(static_cast<void*>(&c)) + offset
				));
		}

		typedef std::function<void(MemberRegister<C>&)> memreg_func;
	};

	// Call lua_toxxx function to get the value on top of the stack and copy to ref.
	// Note it doesn't pop the value.
	template <typename T>
	void LuaToC(lua_State* L, T& ref);
	// Push the value of ref to stack
	template <typename T>
	void CToLua(lua_State* L, T& ref);

	namespace lua_params
	{
		// gens<n>::type = seq<0, 1, ..., n-1>
		template<int ...>
		struct seq {};

		template<int N, int ...S>
		struct gens : gens<N - 1, N - 1, S...> { };

		template<int ...S>
		struct gens<0, S...> {
			typedef seq<S...> type;
		};

		template <typename T>
		T get_param_from_lua(lua_State* L, int n)
		{
			T ret;
			lua_pushvalue(L, n);
			LuaToC<T>(L, ret);
			return ret;
		}

		template <typename Func, typename Ret, typename... Args, int... N>
		Ret call_func(lua_State* L, Func func, seq<N...>)
		{
			// This function is called with a table on top of the stack,
			// first we call table.unpack to get the elements in the table
			lua_getglobal(L, "table");
			lua_getfield(L, -1, "unpack");
			lua_remove(L, -2);    // now function is pushed
			lua_pushvalue(L, -2); // push the table
			lua_call(L, 1, sizeof...(N)); // we only receive sizeof...(N) results

			// Then we call func with get_param_from_lua
			int start_from = lua_gettop(L) - sizeof...(N)+1;
			// Note that N... itself starts from 0
			return func(std::forward<Args>(get_param_from_lua<Args>(L, N + start_from))...);
		}

		// std::function version
		template <typename Ret, typename... Args>
		Ret call_func(lua_State* L, const std::function<Ret(Args...)>& func)
		{
			return call_func<std::function<Ret(Args...)>, Ret, Args...>(L, func, gens<sizeof...(Args)>::type());
		}
	}

	template <typename C, typename... Args>
	std::function<C(lua_State*)> wrap_ctor(const std::function<C(Args...)>& func)
	{
		return [func](lua_State* L) {
			return lua_params::call_func<C, Args...>(L, func);
		};
	}

	template <typename C>
	std::function<C(lua_State*)> wrap_ctor_no_args(C(*func)())
	{
		return [func](lua_State* L) {
			return lua_params::call_func<C>(L, func);
		};
	}

	template <typename C>
	std::function<C(lua_State*)> use_default_ctor()
	{
		static_assert(std::is_constructible<C>::value, "Not able to use default ctor here");
		return [](lua_State* L) {
			return C();
		};
	}

	template <typename C, typename Str, typename Ctor>
	void export_component(Str name, Ctor ctor, const std::function<void(MemberRegister<C>&)>& memreg)
	{
		static_assert(std::is_constructible<std::string, Str>::value, "Bad type of name");
		static_assert(std::is_same<Ctor, std::function<C(lua_State*)>>::value, "Bad type of ctor");

		// Create memreg as a std::function
		//ComponentHelper<C>::memreg_func memreg_func_save = memreg;

		std::function<void(lua_State*)> create_component = [ctor, memreg](lua_State* L) {
			// This function is called with (name, entity_manager_weak_pointer, id, params)
			EntityManagerSharedPtr* m = static_cast<EntityManagerSharedPtr*>(lua_touserdata(L, 2));
			entityx::Entity::Id* id = static_cast<entityx::Entity::Id*>(lua_touserdata(L, 3));
			entityx::Entity entity = (*m)->get(*id);

			typedef ComponentHelper<C>::ptr CompPtr;
			// Create the c++ component
			CompPtr component = entity.assign<C>(std::move(ctor(L)));

			// Create the component as a lightuserdata (although no data is stored in it)
			lua_pushlightuserdata(L, 0);

			// Push CompPtr which will be used in MemberRegister (will be removed after pushing members)
			lua_newuserdata(L, sizeof(CompPtr));
			new (lua_touserdata(L, -1)) CompPtr(component);

			// If needed, create metatable for it (we only need __gc)
			if (!std::is_trivially_destructible<CompPtr>::value)
			{
				lua_newtable(L);
				lua_pushcfunction(L, [](lua_State* L){
					CompPtr* component = static_cast<CompPtr*>(lua_touserdata(L, -1));
					component->~CompPtr();
					return 0;
				});
				lua_setfield(L, -2, "__gc");
				lua_setmetatable(L, -2);
			}

			// Stack: lightuserdata(0) | CompPtr

			// Create metatable for the lightuserdata
			lua_newtable(L);
			lua_pushvalue(L, -1);
			lua_setfield(L, -2, "__index");

			// Stack: lightuserdata(0) | CompPtr | metatable

			// Now we're ready to push members
			MemberRegister<C> reg(L);
			memreg(reg);

			// Stack: lightuserdata(0) | CompPtr | metatable

			// Remove the CompPtr
			lua_remove(L, -2);
			// Set metatable for the lightuserdata
			lua_setmetatable(L, -2);

			// Leave the result on top of the stack
		};
		components[name] = std::move(create_component);
	}
}}
