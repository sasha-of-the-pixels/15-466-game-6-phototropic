#include "PlayMode.hpp"

#include "DrawLines.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "hex_dump.hpp"

#include "LitColorTextureProgram.hpp"
#include "Mesh.hpp"
#include "Load.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include <random>
#include <array>

GLuint game6_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > game6_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("game6.pnct"));
	game6_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > game6_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("game6.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = game6_meshes->lookup(mesh_name);
		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = game6_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;
	});
});

PlayMode::PlayMode(Client &client_) 
		: client(client_), 
		  scene(*game6_scene) {
	//get pointers to leg for convenience:
	for (auto &transform : scene.transforms) {
		if (transform.name.substr(0, 9) == "vine_purp") {
			purple_vines.emplace_back(&transform);
		}
		else if (transform.name.substr(0, 10) == "vine_green") {
			green_vines.emplace_back(&(transform));
		}
		else if (transform.name == "flower_purp") {
			purple_flower = &transform;
		}
		else if (transform.name == "flower_green") {
			green_flower = &transform;
		}
	}

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	for (uint8_t i = 0; i < 5; i++) {
		for (uint8_t j = 0; j < 5; j++) {
			for (uint8_t k = 0; k < 5; k++) {
				occupied_cells[i][j][k] = false;
			}
		};
	};
	occupied_cells[2][2][0] = true;
};

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.repeat) {
			//ignore repeats
		} 
		else if (evt.key.keysym.sym == SDLK_r) {
			if (phase >= 2) {
				client.connection.send("R"); // R for restart
			}
		}
		else if (phase != 1) {
			return false;
		}
		else if (evt.key.keysym.sym == SDLK_a) {
			controls.left.downs += 1;
			controls.left.pressed = true;
			if (my_turn && curr_vine_pos.x > 0 && !occupied_cells[curr_vine_pos.x-1][curr_vine_pos.y][curr_vine_pos.z]) client.connection.send("ML");
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			controls.right.downs += 1;
			controls.right.pressed = true;
			if (my_turn && curr_vine_pos.x < 4 && !occupied_cells[curr_vine_pos.x+1][curr_vine_pos.y][curr_vine_pos.z]) client.connection.send("MR");
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			controls.up.downs += 1;
			controls.up.pressed = true;
			if (my_turn && curr_vine_pos.z < 4 && !occupied_cells[curr_vine_pos.x][curr_vine_pos.y][curr_vine_pos.z+1]) client.connection.send("MU");
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			controls.down.downs += 1;
			controls.down.pressed = true;
			if (my_turn && curr_vine_pos.z > 0 && !occupied_cells[curr_vine_pos.x][curr_vine_pos.y][curr_vine_pos.z-1]) client.connection.send("MD");
			return true;
		} else if (evt.key.keysym.sym == SDLK_e) {
			controls.jump.downs += 1;
			controls.jump.pressed = true;
			if (my_turn && curr_vine_pos.y < 4 && !occupied_cells[curr_vine_pos.x][curr_vine_pos.y+1][curr_vine_pos.z]) client.connection.send("MF");
			return true;
		} else if (evt.key.keysym.sym == SDLK_q) {
			controls.jump.downs += 1;
			controls.jump.pressed = true;
			if (my_turn && curr_vine_pos.y > 0 && !occupied_cells[curr_vine_pos.x][curr_vine_pos.y-1][curr_vine_pos.z]) client.connection.send("MB");
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			controls.left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			controls.right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			controls.up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			controls.down.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_SPACE) {
			controls.jump.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	//queue data for sending to server:
	controls.send_controls_message(&client.connection);

	//reset button press counters:
	controls.left.downs = 0;
	controls.right.downs = 0;
	controls.up.downs = 0;
	controls.down.downs = 0;
	controls.jump.downs = 0;

	// purple_flower->position = glm::vec3(front_purple ? 2.2 : x_purple - 2., !front_purple ? 2.2 : x_purple - 2., y_purple+0.5);
	// green_flower->position = glm::vec3(!front_green ? -2.3 : x_green - 2., front_green ? -2.3 : x_green - 2., y_green+0.5);
	//send/receive data:
	client.poll([this](Connection *c, Connection::Event event){
		if (event == Connection::OnOpen) {
			std::cout << "[" << c->socket << "] opened" << std::endl;
		} else if (event == Connection::OnClose) {
			std::cout << "[" << c->socket << "] closed (!)" << std::endl;
			throw std::runtime_error("Lost connection to server!");
		} else { assert(event == Connection::OnRecv);
			// std::cout << "[" << c->socket << "] recv'd data. Current buffer:\n" << hex_dump(c->recv_buffer); std::cout.flush(); //DEBUG

			// a move was made
			if (c->recv_buffer[0] == (uint8_t) 'P') {
				vine_count++;
				/// =====================================
				/// || @warning stuuuuuupid code ahead ||
				/// =====================================
				if ((my_turn && am_purple) || (!my_turn && am_green)) {
					// purple player turn
					printf("purple player turn\n");
					char pos = c->recv_buffer[1];
					if (pos == 'L') {
						curr_vine_pos.x -= 1;
						purple_vines[vine_count]->rotation = glm::angleAxis(glm::radians(90.f), glm::vec3(0., 1., 0.));
						purple_vines[vine_count]->position = pos_offset + rot_left_offset + glm::vec3(curr_vine_pos.x, curr_vine_pos.y, curr_vine_pos.z);
					} else if (pos == 'R') {
						curr_vine_pos.x += 1;
						purple_vines[vine_count]->rotation = glm::angleAxis(glm::radians(-90.f), glm::vec3(0., 1., 0.));
						purple_vines[vine_count]->position = pos_offset + rot_right_offset + glm::vec3(curr_vine_pos.x, curr_vine_pos.y, curr_vine_pos.z);
					} else if (pos == 'U') {
						curr_vine_pos.z += 1;
						purple_vines[vine_count]->position = pos_offset + go_up_offset + glm::vec3(curr_vine_pos.x, curr_vine_pos.y, curr_vine_pos.z);
					} else if (pos == 'D') {
						curr_vine_pos.z -= 1;
						purple_vines[vine_count]->position = pos_offset + go_down_offset + glm::vec3(curr_vine_pos.x, curr_vine_pos.y, curr_vine_pos.z);
					} else if (pos == 'F') {
						curr_vine_pos.y += 1;
						purple_vines[vine_count]->rotation = glm::angleAxis(glm::radians(90.f), glm::vec3(1., 0., 0.));
						purple_vines[vine_count]->position = pos_offset + rot_forward_offset + glm::vec3(curr_vine_pos.x, curr_vine_pos.y, curr_vine_pos.z);
					} else if (pos == 'B') {
						curr_vine_pos.y -= 1;
						purple_vines[vine_count]->rotation = glm::angleAxis(glm::radians(-90.f), glm::vec3(1., 0., 0.));
						purple_vines[vine_count]->position = pos_offset + rot_back_offset + glm::vec3(curr_vine_pos.x, curr_vine_pos.y, curr_vine_pos.z);
					}
				} else {
					// green player turn
					char pos = c->recv_buffer[1];
					if (pos == 'L') {
						curr_vine_pos.x -= 1;
						green_vines[vine_count]->rotation = glm::angleAxis(glm::radians(90.f), glm::vec3(0., 1., 0.));
						green_vines[vine_count]->position = pos_offset + rot_left_offset + glm::vec3(curr_vine_pos.x, curr_vine_pos.y, curr_vine_pos.z);
					} else if (pos == 'R') {
						curr_vine_pos.x += 1;
						green_vines[vine_count]->rotation = glm::angleAxis(glm::radians(-90.f), glm::vec3(0., 1., 0.));
						green_vines[vine_count]->position = pos_offset + rot_right_offset + glm::vec3(curr_vine_pos.x, curr_vine_pos.y, curr_vine_pos.z);
					} else if (pos == 'U') {
						curr_vine_pos.z += 1;
						green_vines[vine_count]->position = pos_offset + go_up_offset + glm::vec3(curr_vine_pos.x, curr_vine_pos.y, curr_vine_pos.z);
					} else if (pos == 'D') {
						curr_vine_pos.z -= 1;
						green_vines[vine_count]->position = pos_offset + go_down_offset + glm::vec3(curr_vine_pos.x, curr_vine_pos.y, curr_vine_pos.z);
					} else if (pos == 'F') {
						curr_vine_pos.y += 1;
						green_vines[vine_count]->rotation = glm::angleAxis(glm::radians(90.f), glm::vec3(1., 0., 0.));
						green_vines[vine_count]->position = pos_offset + rot_forward_offset + glm::vec3(curr_vine_pos.x, curr_vine_pos.y, curr_vine_pos.z);
					} else if (pos == 'B') {
						curr_vine_pos.y -= 1;
						green_vines[vine_count]->rotation = glm::angleAxis(glm::radians(-90.f), glm::vec3(1., 0., 0.));
						green_vines[vine_count]->position = pos_offset + rot_back_offset + glm::vec3(curr_vine_pos.x, curr_vine_pos.y, curr_vine_pos.z);
					}
				}
				occupied_cells[curr_vine_pos.x][curr_vine_pos.y][curr_vine_pos.z] = true;
				if (am_purple || am_green) my_turn = !my_turn;

				// check win condition
				if (curr_vine_pos == glm::u8vec3(front_purple ? 4 : x_purple, !front_purple ? 4 : x_purple, y_purple)) {
					// purple flower :o
					i_won += am_purple;
					phase = 2;
				} if (curr_vine_pos == glm::u8vec3(!front_green ? 0 : x_green, front_green ? 0 : x_green, y_green)) {
					i_won += am_green;
					phase = 2;
				}
			} 

			// player initialization ("Handshake")
			if (c->recv_buffer[0] == (uint8_t) 'H') {
				if (c->recv_buffer[1] == 0) {
					am_purple = 1;
					my_turn = 1;
				}
				else if (c->recv_buffer[1] == 1) am_green = 1;
			}
			// begin game (allow player 1 to move) ("Go")
			std::vector<uint8_t> flower_pos;
			if (c->recv_buffer[0] == (uint8_t) 'G'){
				phase = 1;
				flower_pos = {c->recv_buffer[1], c->recv_buffer[2], c->recv_buffer[3], c->recv_buffer[4], c->recv_buffer[5], c->recv_buffer[6] };
			} else if ( (c->recv_buffer[0] == 'H' && c->recv_buffer[2] == 'G')) {
				phase = 1;
				flower_pos = {c->recv_buffer[3], c->recv_buffer[4], c->recv_buffer[5], c->recv_buffer[6], c->recv_buffer[7], c->recv_buffer[8] };
			}
			// actually position flower
			if (flower_pos.size() >= 6) {
				x_purple = flower_pos[0] - 40;
				y_purple = flower_pos[1] - 40;
				front_purple = flower_pos[2] - 40;
				x_green = flower_pos[3] - 40;
				y_green = flower_pos[4] - 40;
				front_green = flower_pos[5] - 40;
				purple_flower->position = glm::vec3(front_purple ? 2.2 : x_purple - 2., !front_purple ? 2.2 : x_purple - 2., y_purple+0.5);
				green_flower->position = glm::vec3(!front_green ? -2.3 : x_green - 2., front_green ? -2.3 : x_green - 2., y_green+0.5);
				purple_flower->rotation = glm::angleAxis(glm::radians(90.f), glm::vec3(!front_purple, front_purple, 0.));
				green_flower->rotation = glm::angleAxis(glm::radians(90.f), glm::vec3(front_green, !front_green, 0.));
			}

			else if (c->recv_buffer[0] == 'R') {
				// restart game state
				i_won = 0;
				my_turn = am_purple;
				phase = 1;

				for (uint8_t i = 0; i < 5; i++) {
					for (uint8_t j = 0; j < 5; j++) {
						for (uint8_t k = 0; k < 5; k++) {
							occupied_cells[i][j][k] = false;
						}
					};
				};
				occupied_cells[2][2][0] = true;
				curr_vine_pos = glm::u8vec3(2., 2., 0.);

				// reset vine positions & rotations
				for (uint16_t i = 0; i <= vine_count; i++) {
					purple_vines[i]->position = glm::vec3(0., 0., 0.25);
					green_vines[i]->position = glm::vec3(0., 0., -1.);
					purple_vines[i]->rotation = glm::angleAxis(0.f, glm::vec3(1., 0., 0.));
					green_vines[i]->rotation = glm::angleAxis(0.f, glm::vec3(1., 0., 0.));
				}
				vine_count = 0;

				// re-randomize flower positions
				flower_pos = {c->recv_buffer[1], c->recv_buffer[2], c->recv_buffer[3], c->recv_buffer[4], c->recv_buffer[5], c->recv_buffer[6] };
				// actually position flower
				if (flower_pos.size() >= 6) {
					x_purple = flower_pos[0] - 40;
					y_purple = flower_pos[1] - 40;
					front_purple = flower_pos[2] - 40;
					x_green = flower_pos[3] - 40;
					y_green = flower_pos[4] - 40;
					front_green = flower_pos[5] - 40;
					purple_flower->position = glm::vec3(front_purple ? 2.2 : x_purple - 2., !front_purple ? 2.2 : x_purple - 2., y_purple+0.5);
					green_flower->position = glm::vec3(!front_green ? -2.3 : x_green - 2., front_green ? -2.3 : x_green - 2., y_green+0.5);
					purple_flower->rotation = glm::angleAxis(glm::radians(90.f), glm::vec3(!front_purple, front_purple, 0.));
					green_flower->rotation = glm::angleAxis(glm::radians(90.f), glm::vec3(front_green, !front_green, 0.));
				}
			}

			bool handled_message;
			try {
				do {
					handled_message = false;
					if (game.recv_state_message(c)) handled_message = true;
				} while (handled_message);
			} catch (std::exception const &e) {
				std::cerr << "[" << c->socket << "] malformed message from server: " << e.what() << std::endl;
				//quit the game:
				throw e;
			}

			c->recv_buffer.clear();
		}
	}, 0.0);
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {

	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.f, 0.006f, 0.02f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// glDisable(GL_DEPTH_TEST);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	GL_ERRORS(); //print any errors produced by this setup code

	if (phase < 2) {
		scene.draw(*camera);
	}
	
	//figure out view transform to center the arena:
	float aspect = float(drawable_size.x) / float(drawable_size.y);
	float scale = std::min(
		2.0f * aspect / (Game::ArenaMax.x - Game::ArenaMin.x + 2.0f * Game::PlayerRadius),
		2.0f / (Game::ArenaMax.y - Game::ArenaMin.y + 2.0f * Game::PlayerRadius)
	);
	glm::vec2 offset = -0.5f * (Game::ArenaMax + Game::ArenaMin);

	glm::mat4 world_to_clip = glm::mat4(
		scale / aspect, 0.0f, 0.0f, offset.x,
		0.0f, scale, 0.0f, offset.y,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	DrawLines lines(world_to_clip);

	//helper:
	auto draw_text = [&](glm::vec2 const &at, std::string const &text, float H, glm::u8vec4 color) {
		lines.draw_text(text,
			glm::vec3(at.x, at.y, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			color);
	};

	glm::u8vec4 player_color = am_green ? glm::u8vec4(0xa0, 0xff, 0xa0, 0xff) : glm::u8vec4(0xcc, 0x70, 0xff, 0xff);
	std::string game_text;
	game_text = "You are the ";
	game_text.append(am_green ? "green" : "purple");
	game_text.append(" player.");
	if (phase == 0) game_text.append(" Waiting for green player.");
	else if (phase == 2) {
		game_text.append(i_won ? " You win! ^-^" : "You lose ;-;");
		draw_text(glm::vec2(-0.5f, 0.f), "Press R to restart", 0.15f, player_color);
	}
	else if (my_turn) {
		game_text.append(" Your move!");
	} else {
		game_text.append(" Opponent's move.");
	}
	draw_text(glm::vec2(-1.4f, -1.f + Game::PlayerRadius), game_text, 0.15f, player_color);

	if (phase < 2) {
		lines.draw(glm::vec3(-1.4, 0.833, 0.), glm::vec3(-1.2, 0.767, 0.), player_color);
		lines.draw(glm::vec3(-1.3, 0.9, 0.), glm::vec3(-1.3, 0.7, 0.), player_color);
		lines.draw(glm::vec3(-1.4, 0.767, 0.), glm::vec3(-1.2, 0.833, 0.), player_color);
		draw_text(glm::vec3(-1.432, 0.823, 0.), "A", 0.04f, player_color);
		draw_text(glm::vec3(-1.18, 0.74, 0.), "D", 0.04f, player_color);
		draw_text(glm::vec3(-1.31, 0.91, 0.), "W", 0.04f, player_color);
		draw_text(glm::vec3(-1.31, 0.64, 0.), "S", 0.04f, player_color);
		draw_text(glm::vec3(-1.18, 0.823, 0.), "E", 0.04f, player_color);
		draw_text(glm::vec3(-1.43, 0.74, 0.), "Q", 0.04f, player_color);
	}

	GL_ERRORS();
}
