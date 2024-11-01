#include "Mode.hpp"

#include "Connection.hpp"
#include "Game.hpp"
#include "Scene.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode(Client &client_);
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking for local player:
	Player::Controls controls;

	//latest game state (from server):
	Game game;

	//last message from server:
	std::string server_message;

	//connection to server:
	Client &client;

	// scene stuff
	Scene scene;
	Scene::Camera *camera = nullptr;

	// vine storage
	const uint8_t max_vines = 125;
	uint8_t vine_count = 0;
	std::vector<Scene::Transform*> purple_vines;
	std::vector<Scene::Transform*> green_vines;

	// vine positioning
	glm::vec3 rot_left_offset = glm::vec3(0.25, 0., 0.5);
	glm::vec3 rot_right_offset = glm::vec3(-0.25, 0., 0.5);
	glm::vec3 go_up_offset = glm::vec3(0., 0., 0.25);
	glm::vec3 go_down_offset = glm::vec3(0., 0., 0.75);
	glm::vec3 rot_forward_offset = glm::vec3(0., -0.25, 0.5);
	glm::vec3 rot_back_offset = glm::vec3(0., 0.25, 0.5);

	glm::vec3 pos_offset = glm::vec3(-2., -2., 0.);
	glm::u8vec3 curr_vine_pos = glm::u8vec3(2, 2, 0);
	std::array<std::array<std::array<bool, 5>, 5>,5> occupied_cells;

	// flower positioning
	Scene::Transform *purple_flower = nullptr;
	Scene::Transform *green_flower = nullptr;

	uint8_t x_purple;
	uint8_t y_purple;
	uint8_t front_purple;
	uint8_t x_green;
	uint8_t y_green;
	uint8_t front_green;

	// phases, roles, turn taking
	bool my_turn = 0;
	bool am_purple = 0;
	bool am_green = 0;
	uint8_t phase = 0; // 0: pre-game. 1: during game. 2: winner screen.
	bool i_won = 0;

	// x, y, z order. players start at (2, 2, 0)
};
