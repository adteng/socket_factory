#ifndef _FACTORY_FORREAD_SOCKET_H_
#define _FACTORY_FORREAD_SOCKET_H_
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <list>
#include <map>
//#include "CommDef.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
using namespace std;
class PackageReceiveData;
class FactoryForReadSocket;
typedef struct _Data_Head
{
	uint32_t iCommand;
	uint32_t iDataLen;//CMD_FRAME_ID ʱ Ϊframeid
	uint32_t iRoomId;
} DataHead;
typedef struct _PackageSendData
{
	int _iSendLen;
	int _iTotalLen;
	unsigned char *pData;
} PackageSendData;
enum LinkFlagValue{FD_SHORT_CONNECT=0,FD_LONG_DATA_IN_CONNECT,FD_UDP_CONNECT,FD_LONG_DATA_OUT_CONNECT,FD_LOCAL_LISTEN_CONNECT};
typedef struct _SocketLinkInfo
{
	int iFd;
	socklen_t sock_len;
	struct sockaddr_in addr;
	LinkFlagValue iLinkFlag;//tcpclientfd=0,serverfd=1,udpfd=2
	int last_act_time;
	int iRoomId;
	bool bReceiving;
	PackageReceiveData *pkg;
	FactoryForReadSocket *factory;
	list<PackageSendData *> listSendData;
} SocketLinkInfo;


class PackageReceiveData
{
public:
	PackageReceiveData();
	virtual ~PackageReceiveData();
	bool Execute();
	SocketLinkInfo *_pStu;	
	int _iRecvLen;
	int _iTotalLen;
	unsigned char *_pData;
protected:
	virtual bool doExecute(){}
	virtual void ReleaseSourse(bool bDoResult);
};

class FactoryForReadSocket
{
	public:
		FactoryForReadSocket();
		virtual ~FactoryForReadSocket();
		void stop();
		void start();
		void addFd(int iFd,LinkFlagValue iLinkFlag,sockaddr_in &addr,socklen_t slen);
		bool addSendData(int iFd,PackageSendData *pPackageSendData);
		void push_back(SocketLinkInfo *pStu)
		{
			//m_ListSocket.push_back(pStu);
			m_ListSocket[pStu->iFd] = pStu;
		}
		void registFun(bool (*headOP)(SocketLinkInfo *,unsigned char*),bool (*dataOP)(SocketLinkInfo *),void (*udpOP)(SocketLinkInfo *,unsigned char*,int),void (*exceptOP)(SocketLinkInfo *))
		{
			m_Fun_SetupSocketHead = headOP;
			m_Fun_FinishSocketData = dataOP;
			m_pFun_UDPSocketPackage = udpOP;
			m_Fun_SocketBreak = exceptOP;
		}
		void cleanSource(SocketLinkInfo *pSocketLinkInfo)
		{
			close(pSocketLinkInfo->iFd);
			delete[] pSocketLinkInfo->pkg->_pData;
			delete pSocketLinkInfo->pkg;
			delete pSocketLinkInfo;
		}
		static void setSocketLinkInfo(SocketLinkInfo *pSocketLinkInfo,int iPkgSize)
		{
			pSocketLinkInfo->pkg->_iTotalLen=iPkgSize;
			pSocketLinkInfo->pkg->_iRecvLen = sizeof(DataHead);
			pSocketLinkInfo->pkg->_pStu = pSocketLinkInfo;
			unsigned char *p = new unsigned char[iPkgSize+1];
			memcpy(p,pSocketLinkInfo->pkg->_pData,pSocketLinkInfo->pkg->_iRecvLen);
			pSocketLinkInfo->pkg->_pData=p;
		}
		void setLog(void (*f)(char *))
		{
			m_logFun = f;
		}
	private:
		int m_max_fd;
		fd_set m_fd_set;
		map<int,SocketLinkInfo *> m_ListSocket;
		pthread_mutex_t m_link_mutex;
		pthread_t m_work_thread1,m_work_thread2;
		bool m_running;	
		bool (*m_Fun_SetupSocketHead)(SocketLinkInfo *,unsigned char*);
		bool (*m_Fun_FinishSocketData)(SocketLinkInfo *);
		void (*m_pFun_UDPSocketPackage)(SocketLinkInfo *,unsigned char*,int iLen);
		void (*m_Fun_SocketBreak)(SocketLinkInfo *);
		void (*m_logFun)(char *);
		static void *beginDoing1(void *arg)
		{
			((FactoryForReadSocket *)arg)->run1();
		}
		static void *beginDoing2(void *arg)
		{
			((FactoryForReadSocket *)arg)->run2();
		}
		bool CheckDeleteDataFun(SocketLinkInfo *pSocketLinkInfo,fd_set &fdset);
		void run1();
		void run2();
};
#endif
