# Lua Bindings for [EntityX](https://github.com/alecthomas/entityx)

This system adds the ability to extend entity logic with Lua scripts.

It has only been tested with entityx 0.1.0. 1.0.0 compatible version is still work in process.

## Example
To use this system, first include the header entityx_lua.h. Create a component Position and export it to lua.
```c++
using namespace entityx;
using namespace entityx::lua;

struct Position : entityx::Component<Position> {
	Position(float x = 0.0f, float y = 0.0f) : x(x), y(y) {}

	float x, y;
};

export_component<Position>("Position",
	// Use wrapped constructor to create an object from lua
	wrap_ctor<Position, float, float>([](float x, float y) {
		return Position(x, y);
	}),
	// Make x and y accessible from lua
	[](MemberRegister<Position>& m) {
		m.add("x", &Position::x);
		m.add("y", &Position::y);
	}
);
```

Create the lua state and export the entity manager.
```c++
lua_State* L = luaL_newstate();
luaL_openlibs(L);
setup_entityx_api(L);

ptr<EventManager> events(new EventManager());
ptr<EntityManager> entities(new EntityManager(events));

new_entity_manager(L, entities, "manager");
```

Then you can use the entityx and the component Position in lua.
```lua
-- create a entity class
Player = entityx.manager:new()
function Player:init(o)
	o = o or {}
	-- this class has a component "Position"
	self.position = self:component("Position", o.position or {0, 0})
end
-- create an instance of this class
p = Player:instance({position = {10, 20}})
-- set Position.x to 5
p.position.x(5)
-- this will print 5, 20
print(p.position.x(), p.position.y())
```

Event system is not supported yet.
