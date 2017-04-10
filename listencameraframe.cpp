#include "FactoryForReadSocket.h"
#include <ejet/WriteDateLog.h>
#include <ejet/UnixSys.h>
#include <set>
FactoryForReadSocket g_ReadSocketData;
int g_iServerSocketFd;
CWriteDateLog g_log("listen_camera");
void UDPSocketPackage(SocketLinkInfo *pSocketLinkInfo,unsigned char *sRecvData,int nRecv);
bool FinishSocketData(SocketLinkInfo *pSocketLinkInfo);
bool SetupSocketHead(SocketLinkInfo *pSocketLinkInfo,unsigned char *sData);
void SocketBreak(SocketLinkInfo *pSocketLinkInfo);
#define CMD_FRAME 0x00000003
#define CMD_LOGIN 0x00000002

using namespace std;

pthread_mutex_t g_data_mutex;
unsigned char *g_buff;
int g_buffLen = 0;
set<int> g_ClientList;

void Log(char *s)
{
	g_log << TIME << s << END;
}

int main(int argc,char *argv[])
{
	Setup_Daemon();	
	g_log.SetFilePath("/home/project/working/log");
	g_log.SetExtFileName("log");
	g_log.SetOutFlag(1); //0:no; 1:file; 2:screen; 3:both
	g_log << TIME << "start v1.0" << END;
	
	g_iServerSocketFd = socket(AF_INET,SOCK_STREAM,0); 
	struct sockaddr_in cli_addr;
	unsigned int iNameLen;
	iNameLen = sizeof(cli_addr);
	int iFlags = fcntl(g_iServerSocketFd, F_GETFL, 0);
	fcntl(g_iServerSocketFd, F_SETFL, iFlags | O_NONBLOCK);
//#ifdef LINUX
	int iOpt;
	socklen_t iLen;
	iOpt = 1;
	iLen = sizeof(iOpt);
	//free the port
	setsockopt(g_iServerSocketFd, SOL_SOCKET, SO_REUSEADDR, (void *)&iOpt, iLen);
//#endif

	struct sockaddr_in serv_addr;
	memset((void *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(9999);
	//bind to the specify port
	if(bind(g_iServerSocketFd,(struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		g_log << TIME << "Server:Can't bind local address!" << END;
		return false;
	}
	//begin listen
	listen(g_iServerSocketFd,5);
	pthread_mutex_init(&g_data_mutex,NULL);
	g_buff = new unsigned char[102400];
	g_ClientList.clear();

	g_ReadSocketData.registFun(SetupSocketHead,FinishSocketData,UDPSocketPackage,SocketBreak);
	g_ReadSocketData.setLog(Log);
	g_ReadSocketData.addFd(g_iServerSocketFd,FD_LOCAL_LISTEN_CONNECT,serv_addr,sizeof(serv_addr));
	g_ReadSocketData.start();
	while(1)
	{
		pthread_mutex_lock(&g_data_mutex);	
		for(set<int>::iterator it = g_ClientList.begin();it!=g_ClientList.end();it++)
		{
			PackageSendData *pPackageSendData = new PackageSendData();
			pPackageSendData->pData = new unsigned char[g_buffLen+1];
			memcpy(pPackageSendData->pData,g_buff,g_buffLen);
			pPackageSendData->_iTotalLen = g_buffLen;
			pPackageSendData->_iSendLen = 0;
			if(!g_ReadSocketData.addSendData(*it,pPackageSendData))
				g_log << TIME << "not find fd:" << *it << " in socket list." << END;
		}
		pthread_mutex_unlock(&g_data_mutex);
		usleep(1000*300);	
	}
}

bool SetupSocketHead(SocketLinkInfo *pSocketLinkInfo,unsigned char *sData)
{
							DataHead *pHead = (DataHead *)sData;
							int iCommand=ntohl(pHead->iCommand);
							switch(iCommand)
							{
			  				case CMD_FRAME:
			  					g_log << TIME << "get connect CMD_FRAME fd:" << pSocketLinkInfo->iFd << END;
			  					if(ntohl(pHead->iDataLen <=0 || ntohl(pHead->iDataLen) >500 * 1024))
			  						{
			  							g_log << TIME << "error frame data! datalen=" << (int)ntohl(pHead->iDataLen) << " command=" << iCommand << " fd=" << pSocketLinkInfo->iFd << " roomid=" << pSocketLinkInfo->iRoomId  << END;
			  							//ConnectionLinkingClose(pSocketLinkInfo->iFd);
			  							close(pSocketLinkInfo->iFd);
			  							//DelFromServerUserList(pSocketLinkInfo->iRoomId);
										delete pSocketLinkInfo;
										return true;
			  						}
			  					pSocketLinkInfo->pkg=new PackageReceiveData();
			  					pSocketLinkInfo->pkg->_pData = sData;
			  					FactoryForReadSocket::setSocketLinkInfo(pSocketLinkInfo,sizeof(DataHead)+ntohl(pHead->iDataLen));
			  					break;
			  				case CMD_LOGIN:
			  					pSocketLinkInfo->pkg=new PackageReceiveData();
			  					pSocketLinkInfo->pkg->_pData = sData;
			  					pthread_mutex_lock(&g_data_mutex);
			  					g_ClientList.insert(pSocketLinkInfo->iFd);
			  					pthread_mutex_unlock(&g_data_mutex);	
			  					g_log << TIME << "get connect CMD_LOGIN:" << pSocketLinkInfo->iFd << END;
			 //cout << "get connect CMD_LOGIN:" << pSocketLinkInfo->iFd << endl;
			  					FactoryForReadSocket::setSocketLinkInfo(pSocketLinkInfo,sizeof(DataHead));
			  					break;
			  				default:
			  					g_log << TIME << "unknow command="<<iCommand << " fd=" << pSocketLinkInfo->iFd << END; 
			  					close(pSocketLinkInfo->iFd);
			  					pthread_mutex_lock(&g_data_mutex);	
			  					g_ClientList.erase(pSocketLinkInfo->iFd);
			  					pthread_mutex_unlock(&g_data_mutex);	
			  					delete pSocketLinkInfo;
			  					return true;
			  				}
			  				return false;
}

bool FinishSocketData(SocketLinkInfo *pSocketLinkInfo)
{
								DataHead *pHead = (DataHead *)pSocketLinkInfo->pkg->_pData;
								switch(ntohl(pHead->iCommand))
								{	
			  					case CMD_FRAME:
									pthread_mutex_lock(&g_data_mutex);	
									memcpy(g_buff,pSocketLinkInfo->pkg->_pData,pSocketLinkInfo->pkg->_iTotalLen);
									g_buffLen = pSocketLinkInfo->pkg->_iTotalLen;
									g_log << TIME << "FinishSocketData fd:" << pSocketLinkInfo->iFd  << " finish len:" << pSocketLinkInfo->pkg->_iTotalLen << END;
									g_log << "head->iDataLen:" << (int)ntohl(pHead->iDataLen) << END; 
									pthread_mutex_unlock(&g_data_mutex);
									delete [] pSocketLinkInfo->pkg->_pData;
									delete pSocketLinkInfo->pkg;
									return false;
			  					default:
			  						g_log << TIME << "unknown data 0:" << (int)ntohl(pHead->iCommand) << END; 
			  						close(pSocketLinkInfo->iFd);
			  						delete[] pSocketLinkInfo->pkg->_pData;
			  						delete pSocketLinkInfo->pkg;
			  						delete pSocketLinkInfo;
			  						return true;
								}
								return false;
}
void UDPSocketPackage(SocketLinkInfo *pSocketLinkInfo,unsigned char *sRecvData,int nRecv)
{
									  				
}
void SocketBreak(SocketLinkInfo *pSocketLinkInfo)
{
	g_log << TIME << "socket " << pSocketLinkInfo->iFd << "  break!!!!!!" << END; 
	pthread_mutex_lock(&g_data_mutex);	
	g_ClientList.erase(pSocketLinkInfo->iFd);
	pthread_mutex_unlock(&g_data_mutex);	
}
