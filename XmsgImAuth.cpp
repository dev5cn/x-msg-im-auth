/*
  Copyright 2019 www.dev5.cn, Inc. dev5@qq.com
 
  This file is part of X-MSG-IM.
 
  X-MSG-IM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  X-MSG-IM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU Affero General Public License
  along with X-MSG-IM.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <libx-msg-im-auth-db.h>
#include <libx-msg-im-auth-msg.h>
#include "XmsgImAuth.h"

XmsgImAuth* XmsgImAuth::inst = new XmsgImAuth();

XmsgImAuth::XmsgImAuth()
{

}

XmsgImAuth* XmsgImAuth::instance()
{
	return XmsgImAuth::inst;
}

bool XmsgImAuth::start(const char* path)
{
	Log::setInfo();
	shared_ptr<XmsgImAuthCfg> cfg = XmsgImAuthCfg::load(path);
	if (cfg == nullptr)
		return false;
	Log::setLevel(cfg->cfgPb->log().level().c_str());
	Log::setOutput(cfg->cfgPb->log().output());
	Xsc::init();
	if (!XmsgImAuthDb::instance()->load())
		return false;
	vector<shared_ptr<XscServer>> servers;
	if (cfg->pubXscTcpServerCfg() != nullptr) 
	{
		shared_ptr<XscTcpServer> pubTcpServer(new XscTcpServer(cfg->cfgPb->cgt(), shared_ptr<XmsgImAuthTcpLog>(new XmsgImAuthTcpLog()))); 
		if (!pubTcpServer->startup(cfg->pubXscTcpServerCfg()))
			return false;
		servers.push_back(pubTcpServer);
	}
	if (cfg->pubXscHttpServerCfg() != nullptr) 
	{
		shared_ptr<XscHttpServer> pubHttpServer(new XscHttpServer(cfg->cfgPb->cgt(), shared_ptr<XmsgImAuthHttpLog>(new XmsgImAuthHttpLog()))); 
		if (!pubHttpServer->startup(cfg->pubXscHttpServerCfg()))
			return false;
		servers.push_back(pubHttpServer);
	}
	if (cfg->pubXscWebSocketServerCfg() != nullptr) 
	{
		shared_ptr<XscWebSocketServer> pubWebSocketServer(new XscWebSocketServer(cfg->cfgPb->cgt(), shared_ptr<XmsgImAuthWebSocketLog>(new XmsgImAuthWebSocketLog()))); 
		if (!pubWebSocketServer->startup(cfg->pubXscWebSocketServerCfg()))
			return false;
		servers.push_back(pubWebSocketServer);
	}
	shared_ptr<XscTcpServer> priServer(new XscTcpServer(cfg->cfgPb->cgt(), shared_ptr<XmsgImAuthTcpLog>(new XmsgImAuthTcpLog()))); 
	if (!priServer->startup(cfg->priXscTcpServerCfg()))
		return false;
	vector<shared_ptr<XmsgImN2HMsgMgr>> pubMsgMgrs;
	for (auto& it : servers)
		pubMsgMgrs.push_back(shared_ptr<XmsgImN2HMsgMgr>(new XmsgImN2HMsgMgr(it)));
	XmsgImAuthMsg::init(pubMsgMgrs, shared_ptr<XmsgImN2HMsgMgr>(new XmsgImN2HMsgMgr(priServer))); 
	for (auto& it : servers)
	{
		if (!it->publish())
			return false;
	}
	if (!priServer->publish()) 
		return false;
	if (!this->connect2ne(priServer))
		return false;
	Xsc::hold([](ullong now)
	{
		XmsgImAuth::job(now);
	});
	return true;
}

bool XmsgImAuth::connect2ne(shared_ptr<XscTcpServer> tcpServer)
{
	for (int i = 0; i < XmsgImAuthCfg::instance()->cfgPb->h2n_size(); ++i)
	{
		auto& ne = XmsgImAuthCfg::instance()->cfgPb->h2n(i);
		if (ne.neg() == X_MSG_AP)
		{
			shared_ptr<XmsgAp> ap(new XmsgAp(tcpServer, ne.addr(), ne.pwd(), ne.alg()));
			ap->connect();
			continue;
		}
		LOG_ERROR("unsupported network element group: %s", ne.ShortDebugString().c_str())
		return false;
	}
	return true;
}

void XmsgImAuth::job(ullong now)
{
	XmsgImAuthDb::instance()->future([now]
	{
		XmsgImAuthTokenCollOper::instance()->job2deleteExpiredToken(now);
	});
}

XmsgImAuth::~XmsgImAuth()
{

}

