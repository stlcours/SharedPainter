/*                                                                                                                                           
* Copyright (c) 2012, Eunhyuk Kim(gunoodaddy) 
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*   * Redistributions of source code must retain the above copyright notice,
*     this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above copyright
*     notice, this list of conditions and the following disclaimer in the
*     documentation and/or other materials provided with the distribution.
*   * Neither the name of Redis nor the names of its contributors may be used
*     to endorse or promote products derived from this software without
*     specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "INetPeerEvent.h"

class CNetBroadCastSession : public boost::enable_shared_from_this<CNetBroadCastSession>
{
public:
	static const int DEFAULT_SEND_SEC = 3;

	CNetBroadCastSession( boost::asio::io_service& io_service ) 
		: io_service_(io_service), socket_(io_service), evtTarget_(NULL)
		, broadCastPort_(0), sendMsgSecond_(DEFAULT_SEND_SEC), sentCount_(0)
		, stopBroadCastMsgFlag_(true), broadcast_timer_(io_service)
	{
		//qDebug() << "CNetBroadCastSession" << this;
	}

	~CNetBroadCastSession(void)
	{
		//qDebug() << "~CNetBroadCastSession" << this;
		close();
	}

	void setEvent( INetBroadCastSessionEvent * evt )
	{
		evtTarget_ = evt;
	}

	void setBroadCastMessage( const std::string &msg )
	{
		broadCastMsg_ = msg;
	}

	void sendData( int port, const std::string &data )
	{
		boost::asio::ip::udp::endpoint senderEndpoint( boost::asio::ip::address_v4::broadcast(), port ); 
		
		if( socket_.is_open() )
		{
			socket_.send_to( boost::asio::buffer(data), senderEndpoint); 
		}
	}

	bool openUdp( void )
	{
		if( socket_.is_open() )
			return false;

		boost::system::error_code error; 
		socket_.open( boost::asio::ip::udp::v4(), error ); 

		if (!error) 
		{ 
			socket_.set_option( boost::asio::ip::udp::socket::reuse_address(true) ); 
			socket_.set_option( boost::asio::socket_base::broadcast(true) ); 
			return true;
		} 
	
		return false;
	}

	bool listenUdp( int port )
	{
		try
		{
			if( !openUdp() )
				return false;

			boost::asio::ip::udp::endpoint listen_endpoint( boost::asio::ip::address_v4::any(), port); 
			socket_.bind( listen_endpoint ); 
		}catch(...) 
		{
			return false;
		}

		_start_receive_from();
		return true;
	}

	void close( void )
	{
		// close broadcast udp socket 
		socket_.close();

		// cancel timer
		stopBroadCastMsgFlag_ = true;	
		broadcast_timer_.cancel();
	}

	bool startSend( int port, const std::string &msg, int second = 0 )
	{
		broadCastPort_ = port;

		if( socket_.is_open() || openUdp() )
		{
			stopBroadCastMsgFlag_ = false;

			sentCount_ = 0;
			setBroadCastMessage(msg);

			if( second <= 0 )
				second = DEFAULT_SEND_SEC;

			_start_broadcast_timer( second, true );
			return true;
		} 

		return false;
	}

	void pauseSend( void )
	{
		sentCount_ = 0;
		broadcast_timer_.cancel();
		stopBroadCastMsgFlag_ = true;
	}

	void resumeSend( void )
	{
		stopBroadCastMsgFlag_ = false;
		_start_broadcast_timer( sendMsgSecond_, false );
	}

	void _start_receive_from( void )
	{
		socket_.async_receive_from( 
			boost::asio::buffer(read_buffer_, _BUF_SIZE), sender_endpoint_, 
			boost::bind(&CNetBroadCastSession::_handle_receive_from, shared_from_this(), 
			boost::asio::placeholders::error, 
			boost::asio::placeholders::bytes_transferred)); 
	}

	void _handle_receive_from(const boost::system::error_code& error, size_t bytes_recvd) 
	{ 
		if( !error )
		{
			//last_sender_endpoint_ = sender_endpoint_;	// NOT WORKING
			fireReceiveEvent( read_buffer_, bytes_recvd );

			_start_receive_from();
		}
	}

	void _start_broadcast_timer( int second, bool first )
	{
		sendMsgSecond_ = second;

		if( ! first )
			broadcast_timer_.expires_at( broadcast_timer_.expires_at() + boost::posix_time::seconds(second) );
		else
		{
			broadcast_timer_.cancel();
			broadcast_timer_.expires_from_now( boost::posix_time::seconds(second) );
		}
		broadcast_timer_.async_wait( 
			boost::bind(&CNetBroadCastSession::_handle_broadcast_timer,
			shared_from_this(), boost::asio::placeholders::error) );
	}

	void _handle_broadcast_timer( const boost::system::error_code& e )
	{
		if( stopBroadCastMsgFlag_ )
			return;

		if( e )
		{
			qDebug() << "_handle_broadcast_timer : timer error";
			return;
		}
			
		if( sentCount_ < 100 )	// prevent from exceptional too much send to local network
		{
			sendData( broadCastPort_, broadCastMsg_ );
			sentCount_++;				
			fireSentMessageEvent();
		}

		_start_broadcast_timer( sendMsgSecond_, false );
	}


private:
	void fireReceiveEvent( const char * buffer, size_t len )
	{
		if( evtTarget_ )
		{
			std::string tempbuffer( buffer, len );
			evtTarget_->onINetBroadCastSessionEvent_BroadCastReceived( this, tempbuffer );
		}
	}

	void fireSentMessageEvent( void )
	{
		if( evtTarget_ )
		{
			evtTarget_->onINetBroadCastSessionEvent_SentMessage( this, sentCount_ );
		}
	}

private:
	static const int _BUF_SIZE = 4096;
	boost::asio::io_service& io_service_;
	boost::asio::ip::udp::socket socket_;
	INetBroadCastSessionEvent *evtTarget_;

	int broadCastPort_;
	int sendMsgSecond_;
	int sentCount_;
	bool stopBroadCastMsgFlag_;
	std::string broadCastMsg_;
	boost::asio::deadline_timer broadcast_timer_;
	boost::asio::ip::udp::endpoint sender_endpoint_;
	char read_buffer_[_BUF_SIZE];
};
