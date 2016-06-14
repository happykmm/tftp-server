#include "Connection.h"
#include <time.h>



Connection::Connection(SOCKET *socket, SOCKADDR_IN addr, char *message, int length)
{
	fileError = false;
	blksize = 512;
	finished = false;
	this->socket = socket;
	this->addr = addr;
	Package *pac = (Package *)message;
	unsigned short op = ntohs(pac->opCode);
	int i = 2;
	
	for (i = 2; i < length; i++) {
		if (message[i] == 0) {
			break;
		}
	}
	filename = (char *)malloc(i - 1);
	strcpy(filename, message + 2);
	strcpy(mode, message + 2 + strlen(filename) + 1);
	int index = 2 + strlen(filename) + 1 + strlen(mode) + 1;
	while (index < length) {
		char *x = (char *)malloc(strlen(message + index) + 1);
		index += strlen(x) + 1;
		char *y = (char *)malloc(strlen(message + index) + 1);
		index += strlen(y) + 1;
		if (strcmp(x, "blksize") == 0) {
			sscanf(y, "%d", &blksize);
		}
	}

	if (op == TFTP_OP_READ) {
		fp = fopen(filename, "r");
		int err = errno;
		if (fp == NULL) {
			if (err == EACCES) {
				sendErr(TFTP_ERR_ACCESS_DENIED);
			}
			else if (err == ENOENT) {
				sendErr(TFTP_ERR_FILE_NOT_FOUND);
			}
			else if (err == EEXIST) {
				sendErr(TFTP_ERR_FILE_ALREADY_EXISTS);
			}
			else {
				sendErr(TFTP_ERR_UNDEFINED);
			}
			fileError = true;
		}
		else {
			sendPackage(1);
		}
	}
	else if (op == TFTP_OP_WRITE) {
		fp = fopen(filename, "w");
		int err = errno;
		if (fp == NULL) {
			if (err == EACCES) {
				sendErr(TFTP_ERR_ACCESS_DENIED);
			}
			else if (err == ENOENT) {
				sendErr(TFTP_ERR_FILE_NOT_FOUND);
			}
			else if (err == EEXIST) {
				sendErr(TFTP_ERR_FILE_ALREADY_EXISTS);
			}
			else {
				sendErr(TFTP_ERR_UNDEFINED);
			}
			fileError = true;
		}
		else {
			sendACK(0);
			packageIndex = 0;
		}
	}
	else {
		sendErr(TFTP_ERR_UNEXPECTED_OPCODE);
	}
}


Connection::~Connection()
{
	if (fp != NULL) {
		fclose(fp);
	}
	free(filename);
}

int Connection::handleAck(unsigned short index) 
{
	if (finished && index == packageIndex) {
		return 1;
	}
	int ret = sendPackage((int)index + 1);
	return ret;
}

int Connection::handleData(char *message, int length)
{
	Package *pac = (Package *)message;
	int index = (int)ntohs(pac->code);
	if (packageIndex + 1 == index) {
		fwrite(message + 4, 1, length - 4, fp);
	}
	else {

	}
	packageIndex = index;
	int ret = sendACK((unsigned short)index);
	if (length - 4 < blksize) {
		return 1;
	}
	else if (ret < 0) {
		return ret;
	}
	else {
		return 0;
	}
}

int Connection::sendPackage(int index)
{
	if (fp == NULL) {
		return -2;
	}
	char *data = (char *)malloc(blksize + 4);
	if (index != packageIndex + 1)
		fseek(fp, (int)(index - 1) * blksize, 0);
	Package *op = (Package *)data;
	op->opCode = htons(TFTP_OP_DATA);
	op->code = htons((unsigned short)index);
	int ret = fread(data + 4, 1, blksize, fp);
	if (ret < 0) {
		int ret = sendErr(TFTP_ERR_UNDEFINED);
	}
	else {
		if (ret < blksize) {
			finished = true;
		}
		int rett = mySend(data, ret + 4);
		if (rett < 0) {
			finished = false;
		}
		else {
			if (finished) {
				packageIndex = index;

			}
		}

		packageIndex = index;
	}
	return 0;
}

int Connection::mySend(char *message, int length)
{
	int ret = sendto(*socket, message, length, 0, (SOCKADDR *)&addr, sizeof(addr));
	if (ret == SOCKET_ERROR) {
		cout << "sendto() failed!" << endl;
		return SOCKET_ERROR;
	}
#ifdef _DEBUG_MODE
	cout << "sendto(): " << ret << endl;
#endif
	return ret;
}


int Connection::sendACK(unsigned short index)
{
	char x[4];
	Package *pac = (Package *)x;
	pac->opCode = htons(TFTP_OP_ACK);
	pac->code = htons(index);
	int ret = mySend(x, 4);
	if (ret == SOCKET_ERROR) {
		cout << "ack error!" << endl;
	}

	return ret;
}

int Connection::sendErr(TFTP_ERROR_CODE errorcode)
{
	char x[100];
	Package *pac = (Package *)x;
	pac->opCode = htons(TFTP_OP_ERR);
	pac->code = htons(errorcode);
	switch (errorcode)
	{
	case TFTP_ERR_UNDEFINED:
		strcpy(x + 4, "tftp error undefined!");
		break;
	case TFTP_ERR_FILE_NOT_FOUND:
		strcpy(x + 4, "tftp error file not found!");
		break;
	case TFTP_ERR_ACCESS_DENIED:
		strcpy(x + 4, "tftp error access denied!");
		break;
	case TFTP_ERR_DISK_FULL:
		strcpy(x + 4, "tftp error disk full!");
		break;
	case TFTP_ERR_UNEXPECTED_OPCODE:
		strcpy(x + 4, "tftp error unexpected opcode!");
		break;
	case TFTP_ERR_UNKNOWN_TRANSFER_ID:
		strcpy(x + 4, "tftp error unknown transfer id!");
		break;
	default:
		break;
	}
	convert(x + 4);
	int ret = mySend(x, strlen(x + 4) + 5);
	if (ret == SOCKET_ERROR) {
		cout << "sendErr() failed!" << endl;
	}

#ifdef _DEBUG_MODE 
	cout << "Err: " << inet_ntoa(addr.sin_addr) << " " <<  x + 4 << endl;
#endif
	return ret;
}

void Connection::convert(char *l)
{
	if (strcmp(mode, "netascii") != 0)
		return;
	while (*l != 0) {
		*l = toascii(*l);
		l++;
	}
}

bool Connection::equals(SOCKADDR_IN from)
{
	if ((addr.sin_addr.S_un.S_addr == from.sin_addr.S_un.S_addr)
		&& (addr.sin_port == from.sin_port))
		return true;
	else
		return false;
}

