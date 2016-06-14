#include "Server.h"


Server::Server(int port) {
	WSADATA wsaData;
	int iResult;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		cout << "Socket Opened Error!" << endl;
		exit(1);
	}

	// Create a SOCKET for connecting to server
	serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (serverSocket == INVALID_SOCKET) {
		cout << "Socket() failed!" << endl;
		WSACleanup();
		exit(1);
	}

	// Set the mode of the socket to be nonblocking
	/*int imode = 1;
	iResult = ioctlsocket(serverSocket, FIONBIO, (u_long *)&imode);
	if (iResult == SOCKET_ERROR) {
	cout << "ioctlsocket() failed!" << endl;
	closesocket(serverSocket);
	WSACleanup();
	exit(1);
	}*/

	SOCKADDR_IN addr;
	addr.sin_addr.S_un.S_addr = INADDR_ANY;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	// Bind to the port
	iResult = bind(serverSocket, (SOCKADDR *)&addr, sizeof(addr));
	if (iResult == SOCKET_ERROR) {
		cout << "Bind() failed!" << endl;
		closesocket(serverSocket);
		WSACleanup();
		exit(1);
	}
	cout << "TFTP server start at 127.0.0.1:" << port << endl;
}



void Server::receive() {
	SOCKADDR_IN address;
	Connection *connection;
	vector<Connection *>::iterator it;
	Package *package;
	char *buffer;
	int iResult,
		packageLength,
		addressLength;
	unsigned short sequenceNumber;

	buffer = (char *)malloc(MAX_BUFFER_LENGTH);

	while (1) {
		addressLength = sizeof(address);
		packageLength = recvfrom(serverSocket, buffer, MAX_BUFFER_LENGTH, 0, (SOCKADDR *)&address, &addressLength);
		package = (Package *)buffer;

		for (it = sessions.begin(); it != sessions.end(); it++) {
			if ((*it)->equals(address)) {
				break;
			}
		}

		if (it != sessions.end()) 
		{
			// Received a package from an existed connection
			connection = *it;
			unsigned short opCode = ntohs(package->opCode);
			switch (opCode) {
			case TFTP_OP_READ:
			case TFTP_OP_WRITE:
				sessions.erase(it);
				delete(connection);
				cout << "[RENew] " << inet_ntoa(address.sin_addr) << " connected." << endl;
				connection = new Connection(&serverSocket, address, buffer, packageLength);
				if (connection->fileError) {
					delete(connection);
				}
				else {
					sessions.push_back(connection);
				}
				break;
			case TFTP_OP_ACK:
				sequenceNumber = ntohs(package->code);
				cout << inet_ntoa(address.sin_addr) << ": ACK " << sequenceNumber << endl;
				iResult = connection->handleAck(sequenceNumber);
				if (iResult == 1) {
					cout << inet_ntoa(address.sin_addr) << ": " << connection->filename << " sent successfully!" << endl;
					sessions.erase(it);
					delete(connection);
				}
				break;
			case TFTP_OP_DATA:
				sequenceNumber = ntohs(package->code);
				cout << inet_ntoa(address.sin_addr) << ": DATA " << sequenceNumber << endl;
				iResult = connection->handleData(buffer, packageLength);
				if (iResult == 1) {
					cout << inet_ntoa(address.sin_addr) << ": " << connection->filename << " save successfully!" << endl;
					sessions.erase(it);
					delete(connection);
				}
				break;
			default:
				cout << inet_ntoa(address.sin_addr) << ": Error " << TFTP_ERR_UNEXPECTED_OPCODE << endl;
				connection->sendErr(TFTP_ERR_UNEXPECTED_OPCODE);
				break;
			}
		}
		else
		{  
			// Establish a new Connection
			if (ntohs(package->opCode) == TFTP_OP_READ || ntohs(package->opCode) == TFTP_OP_WRITE) {
				cout << "[New] " << inet_ntoa(address.sin_addr) << ": Connected." << endl;
				connection = new Connection(&serverSocket, address, buffer, packageLength);
				if (connection->fileError) {
					delete(connection);
				}
				else {
					sessions.push_back(connection);
				}
			}
		}
	}
	


}

