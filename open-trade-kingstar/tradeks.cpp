#include "tradeks.h"

#include "utility.h"
#include "config.h"
#include "ins_list.h"
#include "numset.h"
#include "SerializerTradeBase.h"

#include <fstream>
#include <functional>
#include <iostream>
#include <string>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

traderks::traderks(boost::asio::io_context& ios
	, const std::string& key)
	:m_b_login(false)
	,_key(key)
	,_ios(ios)
	,_out_mq_ptr()
	,_out_mq_name(_key + "_msg_out")
	,_in_mq_ptr()
	,_in_mq_name(_key + "_msg_in")
	,_thread_ptr()	
	,_req_login()
	,m_run_receive_msg(false)
	,m_logined_connIds()
{
}

#pragma region systemlogic

void traderks::Start()
{
	try
	{
		_out_mq_ptr = std::shared_ptr<boost::interprocess::message_queue>
			(new boost::interprocess::message_queue(boost::interprocess::open_only
				, _out_mq_name.c_str()));

		_in_mq_ptr = std::shared_ptr<boost::interprocess::message_queue>
			(new boost::interprocess::message_queue(boost::interprocess::open_only
				, _in_mq_name.c_str()));
	}
	catch (const std::exception& ex)
	{
		Log().WithField("fun","Start")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errmsg",ex.what())
			.Log(LOG_ERROR,"open message queue exception");
	}

	try
	{
		m_run_receive_msg.store(true);
		_thread_ptr = boost::make_shared<boost::thread>(
			boost::bind(&traderks::ReceiveMsg,this,_key));
	}
	catch (const std::exception& ex)
	{
		Log().WithField("fun","Start")
			.WithField("key",_key)
			.WithField("bid",_req_login.bid)
			.WithField("user_name",_req_login.user_name)
			.WithField("errmsg",ex.what())
			.Log(LOG_ERROR,"trade kingstar start ReceiveMsg thread fail");		
	}
}

void traderks::ReceiveMsg(const std::string& key)
{
	std::string strKey = key;
	char buf[MAX_MSG_LENTH + 1];
	unsigned int priority = 0;
	boost::interprocess::message_queue::size_type recvd_size = 0;
	while (m_run_receive_msg.load())
	{
		try
		{
			memset(buf, 0, sizeof(buf));
			boost::posix_time::ptime tm = boost::get_system_time()
				+ boost::posix_time::milliseconds(100);
			bool flag = _in_mq_ptr->timed_receive(buf, sizeof(buf), recvd_size, priority, tm);
			if (!m_run_receive_msg.load())
			{
				break;
			}
			if (!flag)
			{
				_ios.post(boost::bind(&traderks::OnIdle, this));
				continue;
			}
			std::string line = buf;
			if (line.empty())
			{
				continue;
			}

			int connId = -1;
			std::string msg = "";
			int nPos = line.find_first_of('|');
			if ((nPos <= 0) || (nPos + 1 >= line.length()))
			{
				Log().WithField("fun","ReceiveMsg")
					.WithField("key",strKey)
					.WithField("msgcontent",line)
					.Log(LOG_ERROR,"trader kingstar ReceiveMsg is invalid!");				
				continue;
			}
			else
			{
				std::string strId = line.substr(0, nPos);
				connId = atoi(strId.c_str());
				msg = line.substr(nPos + 1);
				std::shared_ptr<std::string> msg_ptr=std::make_shared<std::string>(msg);
				_ios.post(boost::bind(&traderks::ProcessInMsg
					, this, connId, msg_ptr));
			}
		}
		catch (const std::exception& ex)
		{
			Log().WithField("fun","ReceiveMsg")
				.WithField("key",strKey)
				.WithField("errmsg",ex.what())
				.Log(LOG_ERROR,"trader kingstar ReceiveMsg exception!");		
			break;
		}
	}
}

void traderks::Stop()
{
	if (nullptr != _thread_ptr)
	{
		m_run_receive_msg.store(false);
		_thread_ptr->join();
	}
}

void traderks::OnIdle()
{
	//TODO::����ʱ�Ĳ�ѯ����ͷ��������¼�
}

void traderks::CloseConnection(int nId)
{
	Log().WithField("fun", "CloseConnection")
		.WithField("key", _key)
		.WithField("bid", _req_login.bid)
		.WithField("user_name", _req_login.user_name)
		.WithField("connId", nId)
		.Log(LOG_INFO, "trader kingstar  CloseConnection");

	for (std::vector<int>::iterator it = m_logined_connIds.begin();
		it != m_logined_connIds.end(); it++)
	{
		if (*it == nId)
		{
			m_logined_connIds.erase(it);
			break;
		}
	}
}

void traderks::ProcessInMsg(int connId, std::shared_ptr<std::string> msg_ptr)
{
	if (nullptr == msg_ptr)
	{
		return;
	}
	
	std::string& msg = *msg_ptr;
	//һ���������Ϣ
	if (msg == CLOSE_CONNECTION_MSG)
	{
		CloseConnection(connId);		
		return;
	}

	SerializerTradeBase ss;
	if (!ss.FromString(msg.c_str()))
	{
		Log().WithField("fun", "ProcessInMsg")
			.WithField("key", _key)
			.WithField("bid", _req_login.bid)
			.WithField("user_name", _req_login.user_name)
			.WithField("msgcontent", msg)
			.WithField("connId", connId)
			.Log(LOG_WARNING, "trader kingstar parse json fail");		
		return;
	}
	ReqLogin req;
	ss.ToVar(req);	
	if (req.aid == "req_login")
	{
		Log().WithField("fun", "ProcessInMsg")
			.WithField("key", _key)
			.WithField("bid", _req_login.bid)
			.WithField("user_name", _req_login.user_name)			
			.WithField("connId", connId)
			.Log(LOG_INFO,"trader kingstar receive login msg ");

		//TODO::�����¼����
		return;
	}
	else if (req.aid == "change_password")
	{
		//TODO::�����޸���������
		return;
	}
	else
	{
		std::string aid = req.aid;
		if (aid == "peek_message")
		{
			//TODO::����peek_message����
			return;
		}
		else if (aid == "insert_order")
		{
			//TODO::����������
			return;
		}
		else if (aid == "cancel_order")
		{
			//TODO::����������
			return;
		}
		else if (aid == "req_transfer")
		{
			//TODO::����ת������
			return;
		}
		else if (aid == "confirm_settlement")
		{
			//TODO::����ȷ�Ͻ��㵥����
			return;
		}
		else if (aid == "qry_settlement_info")
		{
			//TODO::�����ѯ��ʷ������Ϣ����
			return;
		}
		else if (aid == "qry_transfer_serial")
		{
			//TODO::�����ѯת����ˮ����
			return;
		}
		else if (aid == "qry_account_info")
		{
			//TODO::�����ѯ�ʽ��˻�����
			return;
		}
		else if (aid == "qry_account_register")
		{
			//TODO::�����ѯǩԼ��������
			return;
		}
		else if (aid == "insert_condition_order")
		{
			//TODO::�����ѯ��������������
			return;
		}
		else if (aid == "cancel_condition_order")
		{
			//TODO::����ɾ������������
			return;
		}
		else if (aid == "pause_condition_order")
		{
			//TODO::������ͣ����������
			return;
		}
		else if (aid == "resume_condition_order")
		{
			//TODO::����ָ�����������
			return;
		}
		else if (aid == "qry_his_condition_order")
		{
			//TODO::�����ѯ��ʷ����������
			return;
		}		
	}	
}

#pragma endregion

