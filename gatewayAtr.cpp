#include <cstdlib>
#include <iostream>
#include <vector>
#include <thread>
#include <stdio.h>
#include <string.h>
#include <boost/asio.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/thread/mutex.hpp>
#include "trabalhoFinal.hpp"



using boost::asio::ip::tcp;
using namespace std;
using namespace boost::interprocess;


static const int MAX_LENGTH = 1024;
int c = -1;
int b = -1;

static boost::mutex m;

position_t usuario[5000];
active_users_t ativo;

void gateway_serv_aplicacao(int c) // Funcao para implementar regiao compartilhada de usuarios ativos com Servidor de Aplicacao
{
	shared_memory_object shared_memory(open_only, "UsuariosAtivos", read_write);
	mapped_region UsuariosAtivos(shared_memory, read_write);
	void * addr2 = UsuariosAtivos.get_address();
	active_users_t * memoria = new (addr2) active_users_t;

	memoria->mutex.lock();
	memoria->list[c].id = ativo.list[c].id;
	memoria->list[c].timestamp = ativo.list[c].timestamp;
	memoria->list[c].latitude = ativo.list[c].latitude;
	memoria->list[c].longitude = ativo.list[c].longitude;
	memoria->list[c].speed = ativo.list[c].speed;
	memoria->num_active_users = c + 1;
	memoria->mutex.unlock();
}

void gateway_historiador(position_t position_t) // Funcao que implementa fila de mensagens entre Gateway e Historiador
{
	message_queue mq(open_only, "gateway_historiador");
	mq.send(&position_t, sizeof(position_t), 0);
}

void atualiza_ativos(position_t usuario) //Funcao que insere ou atualiza usuarios ativos na struct
{
	//cout << "usuario: " << usuario << endl;
	//Insere ou edita usuarios ativos		
	bool edicao = false;

	for (int j = 0; j <= b; j++)
	{
		//Edita dados caso usuario ja informou sua posicao anteriormente e nao tenha se deconectado
		if (ativo.list[j].id == usuario.id && ativo.list[j].id != -1) {
			edicao = true;

			ativo.list[j].timestamp = usuario.timestamp;
			ativo.list[j].latitude = usuario.latitude;
			ativo.list[j].longitude = usuario.longitude;
			ativo.list[j].speed = usuario.speed;
		}
	}
	if (edicao == false) { //Insere dados do cliente ativo

		b++;
		ativo.num_active_users = b;
		ativo.list[b].id = usuario.id;
		ativo.list[b].timestamp = usuario.timestamp;
		ativo.list[b].latitude = usuario.latitude;
		ativo.list[b].longitude = usuario.longitude;
		ativo.list[b].speed = usuario.speed;
		
	}
}

void session(tcp::socket* sock) // Trata mensagens recebidas
{
	boost::system::error_code error;
	bool recebendo = true;		// variavel pra sair do loop
	char pos[MAX_LENGTH];

	do {
		// recebe as mensagens com as posições
		sock->read_some(boost::asio::buffer(pos, MAX_LENGTH), error);
		
		if (error == boost::asio::error::eof) // Cliente desconectou
		{
			for (int j = 0; j <= b; j++)
			{
				if (ativo.list[j].id == usuario[c].id && ativo.list[j].id != -1)
				{
					ativo.list[j].id = -1; //Atualiza id do cliente desconectado
					c--;
				}
			}
			recebendo = false;
			//delete sock;
			return;
		}else if (error){
			throw boost::system::system_error(error);
		}

		// Separando os itens da mensagem
		const int MAXTOKENS = 17;					// São necessários 17 tokens
		string hold[MAXTOKENS];
		int countTokens = 0;
		char *currentToken;

		currentToken = strtok(pos, "?"); //Separacao GET /
		hold[countTokens] = currentToken;
		countTokens++;
		while ((currentToken != NULL) && (countTokens < MAXTOKENS)) // Cria um vetor com as strings de cada token
		{
			currentToken = strtok(NULL, "=");
			hold[countTokens] = currentToken;
			countTokens++;
			if (countTokens != MAXTOKENS-1)
			{
				currentToken = strtok(NULL, "&");
			}
			else
				currentToken = strtok(NULL, " ");
			hold[countTokens] = currentToken;
			countTokens++;
		}

		cout << "mensagem recebida do cliente" << endl;
		cout << "[ ";
		for (int i = 0; i < countTokens; ++i)
		{
			cout <<  hold[i] << ", ";
		}
		cout << "]" << endl;
	    
		// Compara o inicio da mensagem 
		if (hold[0] == "GET /")
		{
			std::string::size_type sz;  // alias of size_t
			m.lock();
			c++;
			//Completa dados de posicao do usuario
			usuario[c].id = std::stoi(hold[2], &sz);
			usuario[c].timestamp = std::stod(hold[4], &sz);
			usuario[c].latitude = std::stod(hold[6], &sz);
			usuario[c].longitude = std::stod(hold[8], &sz);
			usuario[c].speed = std::stoi(hold[10], &sz);
						
			atualiza_ativos(usuario[c]); //Atualiza lista de usuarios ativos
			gateway_historiador(usuario[c]); //Envia informacoes para o historiador
			gateway_serv_aplicacao(c); //Envia informacoes para servidor de aplicacao
			m.unlock();
		}
		else
		{
			//encerrar conexao
			return;
		}

	} while (recebendo);

	return;
}

void server(boost::asio::io_service& io_service, short port) //Servidor espera conexoes
{
	cout << "Gateway - Servidor TCP/IP ativo - Porta: " << port << endl;
	tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), port));

	while (true)
	{
		tcp::socket* sock = new tcp::socket(io_service);
		acceptor.accept(*sock);
		std::thread t(session, sock);
		t.detach();
	}
}

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		std::cerr << "Erro! Porta nao informada!" << endl;
		return 1;
	}
		
	boost::asio::io_service io_service;
	using namespace std;
	server(io_service, atoi(argv[1]));
	
	return 0;
}
