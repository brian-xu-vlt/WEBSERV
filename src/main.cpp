
#include "Config/ConfigParser.hpp"
#include "Config/Server.hpp"
#include "HTTP/RequestHandler.hpp"
#include "ServerPool.hpp"
#include "Response.hpp"
#include "ResponseHandler.hpp"
#include "utils/Logger.hpp"
#include "utils/Timer.hpp"
#include <sys/socket.h>
#include <fstream>
#include <sstream>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/uio.h>
#include <unistd.h>
#include <utility>
#include <pthread.h>

#define BUFF_SIZE	128
#define BACKLOG		100 //nb of connection queued when listen is called
#define MAXLINE		1024
#define SERVER_PORT 18000
// #define SERVER_PORT 8080
#define SA struct sockaddr

void	fake_workload( int req_id )	{
	int	sleep_time = rand() & 0xFFFFF;
	if (req_id % 2 == 0)
		sleep_time *= 2;
	std::cout << "Sleep for... " << sleep_time << " us." << std::endl;
	usleep(sleep_time);
}

void*	response_thread(void *respHandler)	{

	static_cast<ResponseHandler *>(respHandler)->processRequest();

	return NULL;
}

void	conn_reader(int connfd) {

	char	recvline[MAXLINE+1];
	int		n;
	memset(recvline, 0, MAXLINE);
	n = read(connfd, recvline, MAXLINE-1);
	if (n < 0)	{
		Logger::log("read() returned an error.");
		exit (1);
	}

	RequestHandler	handler;
	std::string		request(recvline);
	std::cout << GREEN << std::endl;
	std::cout << request << std::endl;
	std::cout << NC << std::endl;

	std::cout << BLUE << std::endl;
	RequestHandler::result_type reqResult = handler.update(request.c_str(), request.length());
	std::cout << NC << std::endl;

	// ! Here for Calixte ! //
	ResponseHandler		respHandler(reqResult, SERVER_PORT);

	//threaded version :
	// pthread_t	t;
	// pthread_create(&t, NULL, &response_thread, &respHandler);

	// Wait for the response to be ready
	// while (respHandler.isHeadReady() == false) {
		// std::cout << "Waiting for response to be processed by thread..." << std::endl;
	// }

	respHandler.processRequest();
	int ret = 0;
	while ( (ret = respHandler.doSend(connfd, 0)) >= 0) {
	}

	// Response const&	responseResult = respHandler.getResponse();
	// std::ostringstream output;
	// output << responseResult;
	// send(connfd, output.str().c_str(), output.str().length(), 0);

	// files::File const &file = responseResult.getFileInst();
	// if (file.isGood()) {
	// 	int fd = file.getFD();
	// 	char buff[BUFF_SIZE];
	// 	bzero(buff, BUFF_SIZE);

	// 	Timer t;
	// 	t.start();
	// 	int ret = 1;
	// 	while (ret > 0) {
	// 		ret = read(fd, buff, BUFF_SIZE);
	// 		send(connfd, buff, ret, 0);
	// 		if (ret > 0)
	// 			bzero(buff, BUFF_SIZE);
	// 	}
	// 	std::cout << "timer: " << t.getTimeElapsed() << "ms. "<< std::endl;
	// }
	// !------------------- //

	close(connfd);
}

void	simple_listener( void ) {

	int					req_counter = 0;
	int					listenfd, connfd;
	struct sockaddr_in	servaddr;
	struct sockaddr_in	servaddr_client;

	// getting the fd of the socket
	if ( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		exit(1);

	int iSetOption = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char*)&iSetOption, sizeof(iSetOption));

	// setup the server address infos before biding
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SERVER_PORT);

	if ( bind(listenfd, (SA*)&servaddr, sizeof(servaddr)) < 0 )
		exit(2);
	if ( listen(listenfd, BACKLOG) < 0 )
		exit(3);

	// infinit loop to listen to the port and respond
	while (true)	{

		std::ostringstream message;
		message << "[# " << req_counter++ <<"]" << " Waiting for connections on port " << SERVER_PORT;
		Logger::log(message.str(), Logger::toConsole);

		bzero(&servaddr, sizeof(servaddr_client));
		int len = sizeof(servaddr_client);

		// will block until something comes to the port
		connfd = accept(listenfd, (struct sockaddr *)&servaddr_client, (socklen_t *)&len);

		char client_addr[32];
		bzero(&client_addr, 32);
		inet_ntop(AF_INET, &(servaddr_client.sin_addr), client_addr, 32);
		message.str("");
		message.clear();
		message << "New Connection: Client ip is: " << client_addr;
		Logger::log(message.str());

		conn_reader(connfd);

	} //-- end while
}

void exit_server(int sig) {
	(void)sig;
	std::cout << "\rGot signal, Bye..." << std::endl;
	exit(0);
}

	Params(): config_file("webserv.config"), request("request.test") { }
};

Result<Params>	parse(int ac, char **av)
{
	signal(SIGINT, &exit_server);
	std::string 	path;
	Params			params;

	for (int i = 0; i < ac; i++) {
		std::string arg = std::string(av[i]);
		if (arg == "-c")
		{
			if (i == ac - 1)
				return Result<Params>::err(DefaultError("missing argument"));
			params.config_file = std::string(av[++i]);
		}
		if (arg == "-r")
		{
			if (i == ac - 1)
				return Result<Params>::err(DefaultError("missing argument"));
			params.request = std::string(av[++i]);
		}
	}

	files::File::initContentTypes(TYPES_MIME_CONF_PATH);
	network::ServerPool::init(path);
	Logger::getInstance("./logg", Logger::toConsole);

	simple_listener();

	return 0;
}

