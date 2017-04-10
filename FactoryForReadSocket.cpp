	#include <algorithm>
	#include "FactoryForReadSocket.h"
	#include <iostream>
	PackageReceiveData::PackageReceiveData(){}
	PackageReceiveData::~PackageReceiveData(){}
	bool PackageReceiveData::Execute()
	{
		bool bRet = doExecute();
		ReleaseSourse(bRet);
		return true;
	}
	void PackageReceiveData::ReleaseSourse(bool bDoResult)
	{
		delete [] _pData;
		close(_pStu->iFd);
		delete _pStu;
	}
	
		FactoryForReadSocket::FactoryForReadSocket()
		{
			m_logFun = NULL;
			pthread_mutex_init(&m_link_mutex,NULL);
			FD_ZERO(&m_fd_set);
		}
		FactoryForReadSocket::~FactoryForReadSocket(){}
		void FactoryForReadSocket::stop()
		{
			m_running = false;
			pthread_join(m_work_thread1,NULL);
			pthread_join(m_work_thread2,NULL);
		}
		void FactoryForReadSocket::start()
		{
			m_running = true;
			pthread_create(&m_work_thread1,NULL,beginDoing1,this);
			pthread_create(&m_work_thread2,NULL,beginDoing2,this);
		}
		void FactoryForReadSocket::addFd(int iFd,LinkFlagValue iLinkFlag,sockaddr_in &addr,socklen_t slen)
		{
			SocketLinkInfo *pServerSocketLinkInfo = new SocketLinkInfo();
			pServerSocketLinkInfo->iFd = iFd;
			pServerSocketLinkInfo->addr = addr;
			pServerSocketLinkInfo->sock_len = slen;
			pServerSocketLinkInfo->iLinkFlag=iLinkFlag;
			pServerSocketLinkInfo->last_act_time=time(NULL);
			pServerSocketLinkInfo->bReceiving = false;
			pServerSocketLinkInfo->pkg=NULL;
			pServerSocketLinkInfo->factory = this;
			push_back(pServerSocketLinkInfo);
			FD_SET(iFd,&m_fd_set);
			if(m_max_fd < iFd) m_max_fd = iFd;
		}

		void FactoryForReadSocket::run1()
		{
			
			while(m_running)
			{
				fd_set fs = m_fd_set;
				struct timeval tox;
				tox.tv_sec = 5;
				tox.tv_usec = 0;
				int iRet = select(m_max_fd+1,&fs,NULL,NULL,&tox);
				if(iRet>0)
				{
					pthread_mutex_lock(&m_link_mutex);	
					for(map<int,SocketLinkInfo *>::iterator it=m_ListSocket.begin();it!=m_ListSocket.end();)
					{
						SocketLinkInfo *pSocketLinkInfo = it->second;
						int iFd = pSocketLinkInfo->iFd;
						if(CheckDeleteDataFun(pSocketLinkInfo,fs))
						{
							FD_CLR(iFd,&m_fd_set);
							m_ListSocket.erase(it++);
						}
						else
							it++;
					}
					pthread_mutex_unlock(&m_link_mutex);	
				}	
			}
		}
		void FactoryForReadSocket::run2()
		{
			while(m_running)
			{
				fd_set fset;
				FD_ZERO(&fset);
				map<int,SocketLinkInfo *>::iterator it;
				pthread_mutex_lock(&m_link_mutex);	
				for(it=m_ListSocket.begin();it!=m_ListSocket.end();it++)
				{
					SocketLinkInfo *pSocketLinkInfo = it->second;
					if(!pSocketLinkInfo->listSendData.empty())
					{
						if(m_max_fd < it->first) m_max_fd = it->first;
						FD_SET(it->first,&fset);
					}
				}
				pthread_mutex_unlock(&m_link_mutex);
				struct timeval tox;
				tox.tv_sec = 1;
				tox.tv_usec = 0;
				int iRet = select(m_max_fd+1,NULL,&fset,NULL,&tox);
//cout << "send select:" << iRet << endl;
				if(iRet>0)
				{
					pthread_mutex_lock(&m_link_mutex);
					for(it=m_ListSocket.begin();it!=m_ListSocket.end();it++)
					{	
						SocketLinkInfo *pSocketLinkInfo = it->second;
						list<PackageSendData *>::iterator it_data = pSocketLinkInfo->listSendData.begin();
						
						if(FD_ISSET(pSocketLinkInfo->iFd,&fset) && it_data!=pSocketLinkInfo->listSendData.end())
						{
							PackageSendData *pPackageSendData = *it_data;
							pPackageSendData->_iSendLen += sendto(pSocketLinkInfo->iFd,pPackageSendData->pData+pPackageSendData->_iSendLen,pPackageSendData->_iTotalLen-pPackageSendData->_iSendLen,0,(sockaddr *)&pSocketLinkInfo->addr,pSocketLinkInfo->sock_len);
							if(pPackageSendData->_iSendLen >= pPackageSendData->_iTotalLen)
							{
								delete [] pPackageSendData->pData;
								delete pPackageSendData;
								pSocketLinkInfo->listSendData.erase(it_data);
							}
						}
					}
					pthread_mutex_unlock(&m_link_mutex);	
				}
				else
					usleep(1000*100);
			}		
		}
bool FactoryForReadSocket::CheckDeleteDataFun(SocketLinkInfo *pSocketLinkInfo,fd_set &fdset)
{
				int iFd = pSocketLinkInfo->iFd;
				if(FD_ISSET(iFd,&fdset))
				{	
					if(pSocketLinkInfo->iLinkFlag == FD_UDP_CONNECT)
					{
						//socklen_t sock_len;
						//struct sockaddr_in addr;
						char sRecvData[1024] = {0};				
						int nRecv = recvfrom(iFd,sRecvData,1024,0,(sockaddr *)&pSocketLinkInfo->addr,&pSocketLinkInfo->sock_len);
						if(nRecv > 0)
							m_pFun_UDPSocketPackage(pSocketLinkInfo,(unsigned char*)sRecvData,nRecv);
						else 
							(*m_logFun)( (char *)"udp socket readlen<1" );
						return false;
					}
					else if(pSocketLinkInfo->iLinkFlag == FD_LOCAL_LISTEN_CONNECT)
			  		{
			  			int sock;
        				sockaddr_in addr;
        				socklen_t slen = sizeof(struct sockaddr_in);
        				if((sock = accept(iFd, (struct sockaddr *)&addr, &slen)) == -1)
        				{
        					(*m_logFun)( (char *)"Socket accept error,exit!" ); 
                			exit(-1);
        				}
        				if(m_max_fd < sock) m_max_fd=sock;
        				linger sLinger;
						sLinger.l_onoff = 1; 
						sLinger.l_linger = 5; 
						setsockopt(sock,SOL_SOCKET,SO_LINGER,(const char*)&sLinger,sizeof(linger));			
						int iFlags = fcntl(sock, F_GETFL, 0);
						fcntl(sock, F_SETFL, iFlags | O_NONBLOCK);
						addFd(sock,FD_SHORT_CONNECT,addr,slen);
						return false;
			  		}
			  		else if(pSocketLinkInfo->bReceiving)
			  		{
			  			int iLen = recv(iFd,pSocketLinkInfo->pkg->_pData + pSocketLinkInfo->pkg->_iRecvLen, pSocketLinkInfo->pkg->_iTotalLen-pSocketLinkInfo->pkg->_iRecvLen,0);
			  			if(iLen<=0)
			  			{
			  				(*m_logFun)( (char *)"read 0 socket close" ); 
			  				m_Fun_SocketBreak(pSocketLinkInfo);
			  				close(iFd);
			  				delete []pSocketLinkInfo->pkg->_pData;
			  				delete pSocketLinkInfo->pkg;
							delete pSocketLinkInfo;
							return true;
			  			}
			  			pSocketLinkInfo->pkg->_iRecvLen+=iLen;
						if(pSocketLinkInfo->pkg->_iRecvLen == pSocketLinkInfo->pkg->_iTotalLen)
		  				{
		  						pSocketLinkInfo->bReceiving = false;
		  						pSocketLinkInfo->last_act_time=time(NULL);								
								return m_Fun_FinishSocketData(pSocketLinkInfo);
			  			}
			  		}
			  		else
			  		{
			  				unsigned char sData[48];
			  				memset(sData,0,48);
			  				int iTotal = 0;
							DataHead *pHead = (DataHead *)sData;
							while(iTotal < (int)sizeof(DataHead))
							{
								int iLen = recv(iFd,sData + iTotal, sizeof(DataHead)-iTotal,0);
								if(iLen <= 0)
								{
									(*m_logFun)( (char *)"recv head socket break" );
									m_Fun_SocketBreak(pSocketLinkInfo);
									close(iFd);
									delete pSocketLinkInfo;
									return true;
								}
								iTotal += iLen;
							}
							pSocketLinkInfo->bReceiving = true;
							return m_Fun_SetupSocketHead(pSocketLinkInfo,(unsigned char*)sData);
			  		}
			  	}
			  	else if(difftime( time(NULL), pSocketLinkInfo->last_act_time) > 120 && FD_LOCAL_LISTEN_CONNECT!=pSocketLinkInfo->iLinkFlag && pSocketLinkInfo->iLinkFlag!=FD_UDP_CONNECT)
			  	{
			  				char sLog[64] = {0};
			  				sprintf(sLog,"too long for act time,socket close fd=%d", pSocketLinkInfo->iFd);
			  				(*m_logFun)(  sLog ); 
			  				m_Fun_SocketBreak(pSocketLinkInfo);
			  				close(pSocketLinkInfo->iFd);
			  				if(pSocketLinkInfo->bReceiving)
			  				{
			  					delete []pSocketLinkInfo->pkg->_pData;
			  					delete pSocketLinkInfo->pkg;
			  				}
							delete pSocketLinkInfo;
							return true;
			  	}
			  	return false;
}
bool FactoryForReadSocket::addSendData(int iFd,PackageSendData *pPackageSendData)
{
	map<int,SocketLinkInfo *>::iterator it = m_ListSocket.find(iFd);
	if(it==m_ListSocket.end())
	{
		delete [] pPackageSendData->pData;
		delete pPackageSendData;
		return false;
	}
	SocketLinkInfo *pSocketLinkInfo = it->second;
	pSocketLinkInfo->listSendData.push_back(pPackageSendData);
	return true;
}
