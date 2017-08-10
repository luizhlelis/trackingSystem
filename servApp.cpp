
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/asio.hpp>
#include "trabalhoFinal.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <utility>
#include <stdio.h>
#include <thread>


using boost::asio::ip::tcp;
using namespace boost::interprocess;
using namespace std;

std::string const req_ativos("REQ_ATIVOS");
std::string const req_hist("REQ_HIST");
int const LIST_SIZE = 1000000;
//shared_memory_object shm;
//mapped_region region;
// Abre as filas p/ comunicacao com o historiador
message_queue servapp_historiador(open_only, "servapp_historiador");
message_queue historiador_servapp(open_only, "historiador_servapp");
void * addr;
active_users_t * activeUsers;


// Separa uma string pelo delimitador e retorna um vetor com cada pedaco da string

std::vector<std::string> split(std::string str, char delimiter) {
    vector<std::string> internal;
    stringstream ss(str);
    string tok;

    while (getline(ss, tok, delimiter)) {
        internal.push_back(tok);
    }
    return internal;
}

void session(tcp::socket sock)
{
    try
    {
        char data[1024];
        std::vector<std::string> msg;
        char data_resp[100000];
        historical_data_request_t historicalDataRequest;
        historical_data_reply_t historicalDataReply;
        unsigned int priority;
        message_queue::size_type received_size;
        int client_id, num_samples;
        tm *ltm = NULL;
        
        for (;;)
        {
            memset(data, 0, sizeof(data));
            boost::system::error_code error;
            //cout << "Antes read_some " << endl;
            sock.read_some(boost::asio::buffer(data), error);
            //cout << "Depois read_some " << endl;
            if (error == boost::asio::error::eof)
                break; // Connection closed cleanly by peer.
            else if (error)
                throw boost::system::system_error(error); // Some other error.
            std::cout << "Mensagem recebida de " << sock.remote_endpoint() << " - Mensagem: " << data << std::endl;
            std::string data_request(data);
            
            // Se a mensagem recebida nao estiver vazia, quebra a mensagem pelo delimitador ';'
            if (!data_request.empty()) {
                msg = split(data_request, '\n');
            }

            // Requisição do tipo usuarios ativos
            if (msg[0].compare(req_ativos) == 0) {
                {
                    activeUsers->mutex.lock();
                    strcpy(data_resp, "ATIVOS;");
                    strcat(data_resp, std::to_string(activeUsers->num_active_users).c_str());
                    for (position_t pos : activeUsers->list) {
                        if (pos.id != -1) {
                            strcat(data_resp, ";");
                            strcat(data_resp, std::to_string(pos.id).c_str());
                            strcat(data_resp, ";");
                            strcat(data_resp, std::to_string(pos.id).c_str());
                        }
                    }
                    strcat(data_resp, "\n");
                    activeUsers->mutex.unlock();
                }
            }
            /* Requisicao do tipo dados historicos. */
            if (msg[0].compare(req_hist) == 0) 
            {
                client_id = std::stoi(msg[1]); // salva id do cliente
                num_samples = std::stoi(msg[2]); // salva numero de samples
                if (num_samples == 1) {
                    // Buscar na lista de clientes ativos
                    position_t pos = activeUsers->list[client_id];
                    strcpy(data_resp, "HIST;1;");
                    strcat(data_resp, std::to_string(pos.id).c_str());
                    strcat(data_resp,";POS;");
                    //formatacao do tim_t para DDMMAAAAHHMMSS
                    ltm = localtime(&pos.timestamp);
                    strcat(data_resp, std::to_string(ltm->tm_mday).c_str());
                    strcat(data_resp, std::to_string(ltm->tm_mon + 1).c_str());
                    strcat(data_resp, std::to_string(ltm->tm_year+1900).c_str());
                    strcat(data_resp, std::to_string(ltm->tm_hour + 1).c_str());
                    strcat(data_resp, std::to_string(ltm->tm_min + 1).c_str());
                    strcat(data_resp, std::to_string(ltm->tm_sec + 1).c_str());
                    strcat(data_resp,";");
                    strcat(data_resp, std::to_string(pos.latitude).c_str());
                    strcat(data_resp, ";");
                    strcat(data_resp, std::to_string(pos.longitude).c_str());
                    strcat(data_resp, ";");
                    strcat(data_resp,std::to_string(pos.speed).c_str());
                    strcat(data_resp, ";1\n");; // informacao de estado 1- cliente online
                }
                else
                {
                    historicalDataRequest.id = client_id;
                    historicalDataRequest.num_samples = num_samples;
                    // Requisita dados históricos ao historiador
                    servapp_historiador.send(&historicalDataRequest, sizeof(historicalDataRequest), 0);
                    // Recebe dados histÛricos do historiador
                    historiador_servapp.receive(&historicalDataReply, sizeof(historicalDataReply), received_size, priority);
                    std::cout << historicalDataReply.data[0].id << historicalDataReply.data[0].longitude << std::endl;
                    strcat(data_resp, "HIST;");
                    strcat(data_resp, std::to_string(historicalDataReply.num_samples_available).c_str());
                    if (historicalDataReply.num_samples_available != 0) {
                        strcat(data_resp, ";");
                        strcat(data_resp, std::to_string(client_id).c_str());
                        //POSICOES TEM QUE SER ORDENADAS DA MAIS RECENTE PARA MAIS ANTIGA
                        for(position_t pos : historicalDataReply.data){
                            strcat(data_resp, ";POS;");
                            // FORMATACAO DO TIME_T PARA DDMMAAAAHHSS
                            ltm = localtime(&pos.timestamp);
                            strcat(data_resp, std::to_string(ltm->tm_mday).c_str());
                            strcat(data_resp, std::to_string(ltm->tm_mon + 1).c_str());
                            strcat(data_resp, std::to_string(ltm->tm_year + 1900).c_str());
                            strcat(data_resp, std::to_string(ltm->tm_hour + 1).c_str());
                            strcat(data_resp, std::to_string(ltm->tm_min + 1).c_str());
                            strcat(data_resp, std::to_string(ltm->tm_sec + 1).c_str());
                            strcat(data_resp, ";");
                            strcat(data_resp, std::to_string(pos.latitude).c_str());
                            strcat(data_resp, ";");
                            strcat(data_resp, std::to_string(pos.longitude).c_str());
                            strcat(data_resp, ";");
                            strcat(data_resp, std::to_string(pos.speed).c_str());
                            strcat(data_resp, ";1"); // informacao de estado 1- cliente online
                        }
                        strcat(data_resp, "\n");
                    }
                }
            }
            std::cout << "data_resp: " << data_resp;
            boost::asio::write(sock, boost::asio::buffer(data_resp, sizeof(data_resp)));
            msg.clear();
            data_request.clear();
            memset(data, 0, sizeof(data));
            memset(data_resp, 0, sizeof(data_resp));
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception in thread: " << e.what() << "\n";
    }
}

void server(boost::asio::io_service& io_service, unsigned short port)
{
    tcp::acceptor a(io_service, tcp::endpoint(tcp::v4(), port));
    while (true)
    {
        tcp::socket sock(io_service);
        cout << "Servidor de aplicacao - TCP/IP ativo - Porta: " << port << endl;
        a.accept(sock);
        std::cout << "Nova conexao de " << sock.remote_endpoint() << std::endl;
        //session(std::move(sock));
        std::thread(session, std::move(sock)).detach();

    }
}

int main(int argc, char* argv[])
{
    shared_memory_object::remove("UsuariosAtivos"); 
    shared_memory_object shm(create_only, "UsuariosAtivos", read_write);

    shm.truncate(sizeof(active_users_t));

    mapped_region region(shm, read_write);

    void * addr = region.get_address();

    activeUsers = new (addr) active_users_t;


    try
    {
        if (argc < 2)
        {
            std::cerr << "Erro! Porta nao informada!" << endl;
            return 1;
        }
        
        boost::asio::io_service io_service;
        server(io_service, std::atoi(argv[1]));
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    
    return 0;
}

