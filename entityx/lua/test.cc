#include "entityx_lua.h"

const char* text_script =
	"Player = entityx.manager:new()\n"
	"function Player:init(o)\n"
	"	o = o or {}\n"
	"	self.position = self:component(\"Position\", o.position or {0, 0})\n"
	"end\n"
	"p = Player:instance({position = {10, 20}})\n"
	"p.position.x(5)\n"
	"print(p.position.x(), p.position.y())\n"
	;

struct Position : entityx::Component<Position> {
	Position(float x = 0.0f, float y = 0.0f) : x(x), y(y) {}

	float x, y;
};

int main()
{
	using namespace entityx;
	using namespace entityx::lua;

	export_component<Position>("Position",
		wrap_ctor<Position, float, float>([](float x, float y) {
			return Position(x, y);
		}),
		[](MemberRegister<Position>& m) {
			m.add("x", &Position::x);
			m.add("y", &Position::y);
		}
	);

	lua_State* L = luaL_newstate();
	luaL_openlibs(L);
	setup_entityx_api(L);

	ptr<EventManager> events(new EventManager());
	ptr<EntityManager> entities(new EntityManager(events));

	new_entity_manager(L, entities, "manager");

	luaL_dostring(L, text_script);

	lua_close(L);
	return 0;
}

