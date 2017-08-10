#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string.h>
#include <string>
#include <bitset>
#include <mutex>
#include <thread>
#include <boost/interprocess/ipc/message_queue.hpp>
#include "trabalhoFinal.hpp"

using namespace boost::interprocess;
using namespace std;
using namespace boost;

mutex m;
static const unsigned MAX_MSG = 100; // Número de mensagens máximas nas filas servapp_historiador, historiador_servapp e  gateway_historiador


/**
 * Processa o pedido procurando o id no banco de dados, se achar retorna em uma estrutura
 * do tipo historical_data_reply_t com os dados pedidos
 */
historical_data_reply_t process_request(historical_data_request_t request)
{
	int num_amostras = 0, pos;
	historical_data_reply_t reply;
	position_t aux[MAX_POSITION_SAMPLES];
	string name;
	fstream file;

	

	name = to_string(request.id) + ".dat";
	m.lock();
	file.open(name, std::fstream::in | std::fstream::binary | std::fstream::ate); 

	if (file.is_open())
	{
		// Imprime a posição atual do ponteiro do arquivo, representando o tamanho dele
		int file_size = file.tellg();

		if(file_size == 0)
		{
			reply.num_samples_available = 0;
			file.close();
			m.unlock();
		}
		else
		{
			// Recupera o número de registros presentes no arquivo
			int n = file_size/sizeof(position_t);

			num_amostras = request.num_samples;

			// Adapta o número de amostras
			if(num_amostras > MAX_POSITION_SAMPLES)
				num_amostras = MAX_POSITION_SAMPLES;
			
			if(num_amostras > n)
				num_amostras = n;

			// Último registro
			pos = n;

			// Percorre todas as amostras
			for (int i = 0; i < num_amostras; ++i)
			{
				// Coloca o cursor no final do registro anterior
				file.seekp((pos-1)*sizeof(position_t), std::ios_base::beg);

				// Lê e salva na resposta
				file.read((char*)&reply.data[i], sizeof(position_t));

				pos--;
			}

			// Número de amostras
			reply.num_samples_available = num_amostras;
			file.close();
			m.unlock();
		}
		
	}
	else
	{
		// Não tem dados disponíveis
		reply.num_samples_available = 0;
		m.unlock();
	}

	return reply;
}

/**
 * Responsável pela troca de dados entre o historiador e o servidor.
 * Gerencia as filas "servapp_historiador" e "historiador_servapp"
 **/
void historico_server()
{
	unsigned int priority;
	message_queue::size_type recvd_size;
	historical_data_request_t request;
	historical_data_reply_t reply;

	// Cria as filas

	// Fila "servapp_historiador"
	message_queue::remove("servapp_historiador"); //Remove caso tenha mensagens antigas
	// Cria a lista de mensagens com nome, número máximo de mensagens e tamanho máximo da mensagem
	message_queue serv_hist(create_only, "servapp_historiador", MAX_MSG, sizeof(historical_data_request_t));
	cout << "Fila 'servapp_historiador' criada." << endl;
	

	// Fila "historiador_servapp"
	message_queue::remove("historiador_servapp"); //Remove caso tenha mensagens antigas
	// Cria a lista de mensagens com nome, número máximo de mensagens e tamanho máximo da mensagem
	message_queue hist_serv(create_only, "historiador_servapp", MAX_MSG, sizeof(historical_data_reply_t));
	cout << "Fila 'historiador_servapp' criada." << endl;

	// Entra no loop
	while(1)
	{
		// Espera receber mensagem do servidor na fila servapp_historiador

		serv_hist.receive(&request, sizeof(request), recvd_size, priority);
		cout << "Mensagem do servidor de aplicação recebida." << endl;

		// Processa o pedido
		reply = process_request(request);
		
		cout << "Mensagem processada." << endl;

		// Adiciona a resposta na fila historiador_servapp
		hist_serv.send(&reply, sizeof(reply), 0);
		cout << "Mensagem enviada para o servapp." << endl;

	}
}

/**
 * Responsável pela troca de dados entre o historiador e o gateway.
 * Gerencia a fila "gateway_historiador".
 **/
void GatewayHist()
{
	position_t cliente;
	fstream file;
	string nome;
	unsigned int priority;
    message_queue::size_type recvd_size;
    message_queue::remove("gateway_historiador");   //Remove caso tenha mensagens antigas

   	// Cria a lista de mensagens com nome, número máximo de mensagens e tamanho máximo da mensagem
	message_queue gateway_historiador
		(open_or_create								//abre ou cria
		, "gateway_historiador"				    	//nome
		, MAX_MSG                        		 	//numero maximo de mensagens (100)
		, sizeof(position_t)                        //tamanho maximo da mensagem( struct position_t)
		);
	cout << "Fila 'gateway_historiador' criada." << endl;
	
    while (1)
    {   	
      	// Espera receber mensagem do gateway na fila gateway_historiador.
    	gateway_historiador.receive(&cliente, sizeof(cliente), recvd_size, priority);
    	cout << "Mensagem do gateway recebida." << endl;
		nome = std::to_string(cliente.id) + ".dat"; // cria o nome do arquivo binario (id.dat)
		m.lock();
		file.open(nome,std::fstream::out | std::fstream::app); // out para gravar, app para ser no fim do arquivo.
		file.write((char*)&cliente, sizeof(position_t)); // escreve no arquivo a estrutura cliente em formato binario.
		cout << "Novos dados do cliente "<< cliente.id << " adicionados ao seu arquivo." << endl;
		file.close(); // fecha arquivo.
		m.unlock();
    }
}


int main()
{
	thread hs(historico_server);
	thread GH(GatewayHist);
	hs.join();
	GH.join();

	return 0;
}