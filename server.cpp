
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

	std::function<void(std::unordered_map<Connection*, Player*>, uint8_t msg)> yap_to_all_players = [] (std::unordered_map<Connection*, Player*> connection_to_player, uint8_t msg) {
		for (auto it = connection_to_player.begin(); it != connection_to_player.end(); it++) {
			Connection *c = it->first;
			c->send(msg);
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
					c->send((char)'H');
					c->send((uint8_t) (connection_to_player.size() + 0x2F));
					printf("sending: %u %u\n", c->send_buffer.data()[0], c->send_buffer.data()[1]);
					printf("send buf size: %lu\n", c->send_buffer.size());

					if (connection_to_player.size() == 2) {
						uint8_t x_purple = rand() % 5 + 40;
						uint8_t y_purple = rand() % 5 + 40;
						uint8_t front_purple = rand() % 2 + 40;
						uint8_t x_green = rand() % 5 + 40;
						uint8_t y_green = rand() % 5 + 40;
						uint8_t front_green = rand() % 2 + 40;
						
						yap_to_all_players(connection_to_player, 0x47);
						yap_to_all_players(connection_to_player, x_purple);
						yap_to_all_players(connection_to_player, y_purple);
						yap_to_all_players(connection_to_player, front_purple);
						yap_to_all_players(connection_to_player, x_green);
						yap_to_all_players(connection_to_player, y_green);
						yap_to_all_players(connection_to_player, front_green);
						printf("sending: %u %u %u\n", c->send_buffer.data()[0], c->send_buffer.data()[1], c->send_buffer.data()[2]);
					
					}

				} else if (evt == Connection::OnClose) {
					//client disconnected:

					remove_connection(c);

				} else { assert(evt == Connection::OnRecv);
					//got data from client:
					//std::cout << "Sending (onRecv):" << c->send_buffer.data() << std::endl; std::cout.flush(); //DEBUG

					//look up in players list:
					auto f = connection_to_player.find(c);
					assert(f != connection_to_player.end());
					Player &player = *f->second;
					printf("receiving: %s\n", c->recv_buffer.data());

					for (uint32_t i = 0; i < c->recv_buffer.size(); i++) {
						if (c->recv_buffer[0] == (uint8_t)'M') {
							if (i == 0) continue;

							char dir = (char) c->recv_buffer[1];
							yap_to_all_players(connection_to_player, (uint8_t) 'P');
							yap_to_all_players(connection_to_player, dir);
							c->recv_buffer.erase(c->recv_buffer.begin());
							c->recv_buffer.erase(c->recv_buffer.begin());
							i=0;
						}

						else if (c->recv_buffer[0] == (uint8_t) 'R') {
							if (i < 6) continue;
							// flower time
							if (connection_to_player.size() == 2) {
								uint8_t x_purple = rand() % 5 + 40;
								uint8_t y_purple = rand() % 5 + 40;
								uint8_t front_purple = rand() % 2 + 40;
								uint8_t x_green = rand() % 5 + 40;
								uint8_t y_green = rand() % 5 + 40;
								uint8_t front_green = rand() % 2 + 40;
								yap_to_all_players(connection_to_player,(uint8_t) 'R');
								yap_to_all_players(connection_to_player, x_purple);
								yap_to_all_players(connection_to_player, y_purple);
								yap_to_all_players(connection_to_player, front_purple);
								yap_to_all_players(connection_to_player, x_green);
								yap_to_all_players(connection_to_player, y_green);
								yap_to_all_players(connection_to_player, front_green);
							}
							c->recv_buffer.erase(c->recv_buffer.begin());
							c->recv_buffer.erase(c->recv_buffer.begin());
							c->recv_buffer.erase(c->recv_buffer.begin());
							c->recv_buffer.erase(c->recv_buffer.begin());
							c->recv_buffer.erase(c->recv_buffer.begin());
							c->recv_buffer.erase(c->recv_buffer.begin());
							c->recv_buffer.erase(c->recv_buffer.begin());
							i = 0;
						}
						// c->recv_buffer.clear();
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
