
#include "Connection.hpp"

#include "hex_dump.hpp"

#include "Game.hpp"

#include <chrono>
#include <stdexcept>
#include <iostream>
#include <cassert>
#include <unordered_map>
#include <string>

#ifdef _WIN32
extern "C" { uint32_t GetACP(); }
#endif
int main(int argc, char **argv) {
#ifdef _WIN32
	{ //when compiled on windows, check that code page is forced to utf-8 (makes file loading/saving work right):
		//see: https://docs.microsoft.com/en-us/windows/apps/design/globalizing/use-utf8-code-page
		uint32_t code_page = GetACP();
		if (code_page == 65001) {
			std::cout << "Code page is properly set to UTF-8." << std::endl;
		} else {
			std::cout << "WARNING: code page is set to " << code_page << " instead of 65001 (UTF-8). Some file handling functions may fail." << std::endl;
		}
	}

	//when compiled on windows, unhandled exceptions don't have their message printed, which can make debugging simple issues difficult.
	try {
#endif

	//------------ argument parsing ------------

	if (argc != 2) {
		std::cerr << "Usage:\n\t./server <port>" << std::endl;
		return 1;
	}

	//------------ initialization ------------

	Server server(argv[1]);

	//------------ main loop ------------

	//keep track of which connection is controlling which player:
	std::unordered_map< Connection *, Player * > connection_to_player;
	//keep track of game state:
	Game game;

	std::function<void(std::unordered_map<Connection*, Player*>, std::string)> yap_to_all_players = [] (std::unordered_map<Connection*, Player*> connection_to_player, std::string msg) {
		for (auto it = connection_to_player.begin(); it != connection_to_player.end(); it++) {
			Connection *c = it->first;
			c->send(msg);
			std::cout << c->send_buffer.data();
		}
	};

	while (true) {
		static auto next_tick = std::chrono::steady_clock::now() + std::chrono::duration< double >(Game::Tick);
		//process incoming data from clients until a tick has elapsed:
		while (true) {
			auto now = std::chrono::steady_clock::now();
			double remain = std::chrono::duration< double >(next_tick - now).count();
			if (remain < 0.0) {
				next_tick += std::chrono::duration< double >(Game::Tick);
				break;
			}

			//helper used on client close (due to quit) and server close (due to error):
			auto remove_connection = [&](Connection *c) {
				auto f = connection_to_player.find(c);
				assert(f != connection_to_player.end());
				game.remove_player(f->second);
				connection_to_player.erase(f);
			};

			server.poll([&](Connection *c, Connection::Event evt){
				if (evt == Connection::OnOpen) {
					//client connected:

					//create some player info for them:
					connection_to_player.emplace(c, game.spawn_player());
					c->send('H'); c->send((uint8_t)(connection_to_player.size()-1));

					if (connection_to_player.size() == 2) {
						uint8_t x_purple = rand() % 5 + 40;
						uint8_t y_purple = rand() % 5 + 40;
						uint8_t front_purple = rand() % 2 + 40;
						uint8_t x_green = rand() % 5 + 40;
						uint8_t y_green = rand() % 5 + 40;
						uint8_t front_green = rand() % 2 + 40;
						std::array<char, 6> flower_pos = {
							(char)x_purple, (char)y_purple, (char)front_purple, 
							(char)x_green, (char)y_green, (char)front_green};
						std::string G = "G";
						std::string flower_pos_str = G.append(std::string(flower_pos.data(), 6));
						yap_to_all_players(connection_to_player, flower_pos_str);
					}

				} else if (evt == Connection::OnClose) {
					//client disconnected:

					remove_connection(c);

				} else { assert(evt == Connection::OnRecv);
					//got data from client:
					//std::cout << "current buffer:\n" << hex_dump(c->recv_buffer); std::cout.flush(); //DEBUG

					//look up in players list:
					auto f = connection_to_player.find(c);
					assert(f != connection_to_player.end());
					Player &player = *f->second;

					if (c->recv_buffer[0] == (uint8_t)'M') {
						std::string msg = "P";
						char dir = (char) c->recv_buffer[1];
						std::string dir2(&dir, 1);
						msg.append(dir2);
						yap_to_all_players(connection_to_player, msg);
						std::cout << c->send_buffer.data() << std::endl;
						c->recv_buffer.clear();
					}

					else if (c->recv_buffer[0] == (uint8_t) 'R') {
						// yap_to_all_players(connection_to_player, "R");
						// flower time
						if (connection_to_player.size() == 2) {
							uint8_t x_purple = rand() % 5 + 40;
							uint8_t y_purple = rand() % 5 + 40;
							uint8_t front_purple = rand() % 2 + 40;
							uint8_t x_green = rand() % 5 + 40;
							uint8_t y_green = rand() % 5 + 40;
							uint8_t front_green = rand() % 2 + 40;
							std::array<char, 6> flower_pos = {
								(char)x_purple, (char)y_purple, (char)front_purple, 
								(char)x_green, (char)y_green, (char)front_green};
							std::string R = "R";
							std::string flower_pos_str = R.append(std::string(flower_pos.data(), 6));
							yap_to_all_players(connection_to_player, flower_pos_str);
						}
						c->recv_buffer.clear();
					}

					//handle messages from client:
					try {
						bool handled_message;
						do {
							handled_message = false;
							if (player.controls.recv_controls_message(c)) handled_message = true;
							//TODO: extend for more message types as needed
						} while (handled_message);
					} catch (std::exception const &e) {
						std::cout << "Disconnecting client:" << e.what() << std::endl;
						c->close();
						remove_connection(c);
					}
				}
			}, remain);
		}

		//update current game state
		game.update(Game::Tick);

		//send updated game state to all clients
		// for (auto &[c, player] : connection_to_player) {
		// 	game.send_state_message(c, player);
		// }

	}


	return 0;

#ifdef _WIN32
	} catch (std::exception const &e) {
		std::cerr << "Unhandled exception:\n" << e.what() << std::endl;
		return 1;
	} catch (...) {
		std::cerr << "Unhandled exception (unknown type)." << std::endl;
		throw;
	}
#endif
}
