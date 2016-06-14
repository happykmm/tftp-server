#include "Server.h"




int main(int argc, char* argv[])
{
	int port = 69;
	Server* server = new Server(port);
	server->receive();
	return 0;
}

