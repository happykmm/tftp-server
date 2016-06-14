#include "stdafx.h"
#include "Protocal.h"
#include "Connection.h"
#include <vector>
#include <time.h>


class Server {
private:
	SOCKET serverSocket;
	vector<Connection *> sessions;

public:
	Server(int port);
	void receive();

};