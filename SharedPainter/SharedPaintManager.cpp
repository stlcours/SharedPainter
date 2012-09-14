#include "StdAfx.h"
#include "SharedPaintManager.h"
#include <QHostInfo>

#define START_SERVER_PORT                       4001
#define DEFAULT_BROADCAST_PORT                  3336
#define DEFAULT_BROADCAST_UDP_PORT_FOR_TEXTMSG  3338
#define START_UDP_LISTEN_PORT	                5001

CSharedPaintManager::CSharedPaintManager(void) : commandMngr_(this), canvas_(NULL), acceptPort_(-1), listenUdpPort_(-1), serverMode_(false), clientMode_(false)
, lastWindowWidth_(0), lastWindowHeight_(0), gridLineSize_(0)
, lastPacketId_(-1)
{
	// default generate my id
	myId_ = Util::generateMyId();

	// create my user info
	struct SPaintUserInfoData data;
	data.userId = myId_;

	myUserInfo_ = boost::shared_ptr<CPaintUser>(new CPaintUser);
	myUserInfo_->setData( data );

	backgroundColor_ = Qt::white;

	broadCastSessionForSendMessage_ = boost::shared_ptr< CNetBroadCastSession >(new CNetBroadCastSession( netRunner_.io_service() ));
	broadCastSessionForSendMessage_->setEvent( this );
	broadCastSessionForSendMessage_->openUdp();

	broadCastSessionForRecvMessage_ = boost::shared_ptr< CNetBroadCastSession >(new CNetBroadCastSession( netRunner_.io_service() ));
	broadCastSessionForRecvMessage_->setEvent( this );
	if( broadCastSessionForRecvMessage_->listenUdp( DEFAULT_BROADCAST_UDP_PORT_FOR_TEXTMSG ) == false )
	{
		// ignore this error.. by multi programs on a computer
	}

	clearAllUsers(); // clear & add my user info
}

CSharedPaintManager::~CSharedPaintManager(void)
{
	close();
}


void CSharedPaintManager::setBroadCastChannel( const std::string & channel )
{
	std::string myIp = Util::getMyIPAddress();
	std::string broadCastMsg = BroadCastPacketBuilder::CProbeServer::make( channel, myIp, listenUdpPort_ );
	
	if( broadCastSessionForConnection_ )
		broadCastSessionForConnection_->setBroadCastMessage( broadCastMsg );
	
	myUserInfo_->setChannel( channel );
	broadcastChannel_ = channel;
}

void CSharedPaintManager::sendBroadCastTextMessage( const std::string &broadcastChannel, const std::string &msg )
{
	std::string data;
	data = BroadCastPacketBuilder::CTextMessage::make( broadcastChannel, myId_, msg );

	broadCastSessionForSendMessage_->sendData( DEFAULT_BROADCAST_UDP_PORT_FOR_TEXTMSG, data );
}

bool CSharedPaintManager::startClient( void )
{
	stopClient();
	stopServer();

	// for receiving server info
	if( udpSessionForConnection_ )
		udpSessionForConnection_->close();
	udpSessionForConnection_ = boost::shared_ptr< CNetUdpSession >(new CNetUdpSession( netRunner_.io_service() ));
	udpSessionForConnection_->setEvent(this);

	listenUdpPort_ = START_UDP_LISTEN_PORT;
	while( true )
	{
		if( udpSessionForConnection_->listen( listenUdpPort_ ) )
			break;
		listenUdpPort_++;
	}

	// broadcast for finding server
	std::string myIp = Util::getMyIPAddress();
	std::string broadCastMsg = BroadCastPacketBuilder::CProbeServer::make( broadcastChannel_, myIp, listenUdpPort_ );

	if( broadCastSessionForConnection_ )
		broadCastSessionForConnection_->close();
	broadCastSessionForConnection_ = boost::shared_ptr< CNetBroadCastSession >(new CNetBroadCastSession( netRunner_.io_service() ));
	broadCastSessionForConnection_->setEvent( this );
	broadCastSessionForConnection_->startSend( DEFAULT_BROADCAST_PORT, broadCastMsg, 3 );

	qDebug() << "startClient" << listenUdpPort_;
	clientMode_ = true;
	return true;
}


bool CSharedPaintManager::startServer( const std::string &broadCastChannel, int port )
{
	stopClient();
	stopServer();

	if( port <= 0 )
		port = START_SERVER_PORT;

	netPeerServer_ = boost::shared_ptr<CNetPeerServer>(new CNetPeerServer( netRunner_.io_service() ));
	netPeerServer_->setEvent( this );

	for( int port = START_SERVER_PORT; port < START_SERVER_PORT + 100; port++ )
	{
		if( netPeerServer_->start( port ) )
		{
			acceptPort_ = port;
			break;
		}
	}
	assert( acceptPort_ > 0 );

	if( broadCastSessionForConnection_ )
		broadCastSessionForConnection_->close();
	broadCastSessionForConnection_ = boost::shared_ptr< CNetBroadCastSession >(new CNetBroadCastSession( netRunner_.io_service() ));
	broadCastSessionForConnection_->setEvent( this );

	if( !broadCastSessionForConnection_->listenUdp( DEFAULT_BROADCAST_PORT ) )
	{
		stopServer();
		return false;
	}

	qDebug() << "startServer" << acceptPort_;
	serverMode_ = true;
	return true;
}


void CSharedPaintManager::stopClient( void )
{
	if( ! clientMode_ )
		return;

	clearAllUsers();
	clearAllSessions();

	if( udpSessionForConnection_ )
		udpSessionForConnection_->close();

	if( broadCastSessionForConnection_ )
		broadCastSessionForConnection_->close();

	clientMode_ = false;
}

void CSharedPaintManager::stopServer( void )
{
	if( ! serverMode_ )
		return;

	clearAllUsers();
	clearAllSessions();

	if( netPeerServer_ )
		netPeerServer_->close();

	if( broadCastSessionForConnection_ )
		broadCastSessionForConnection_->close();

	serverMode_ = false;
}


void CSharedPaintManager::deserializeData( const char * data, size_t size )
{
	CPacketSlicer slicer;
	slicer.addBuffer( data, size );

	if( slicer.parse() == false )
		return;

	for( size_t i = 0; i < slicer.parsedItemCount(); i++ )
	{
		boost::shared_ptr<CPacketData> data = slicer.parsedItem( i );
		dispatchPaintPacket( boost::shared_ptr<CPaintSession>() /*NULL*/, data );
	}

	std::string allData(data, size);
	sendDataToUsers( allData );
}


std::string CSharedPaintManager::serializeData( const std::string *target )
{
	std::string allData;

	// Window Resize
	std::string msg = WindowPacketBuilder::CResizeMainWindow::make( lastWindowWidth_, lastWindowHeight_, target );
	allData += msg;

	// Background Grid Line
	if( gridLineSize_ > 0 )
		allData += PaintPacketBuilder::CSetBackgroundGridLine::make( gridLineSize_, target );

	// Background Color
	if( backgroundColor_ != Qt::white )
		allData += PaintPacketBuilder::CSetBackgroundColor::make( backgroundColor_.red(), backgroundColor_.green(), backgroundColor_.blue(), backgroundColor_.alpha(), target );

	// Background Image
	if( backgroundImageItem_ )
		allData += PaintPacketBuilder::CSetBackgroundImage::make( backgroundImageItem_, target );

	// History all paint item
	commandMngr_.lock();
	size_t itemSize = 0;
	const ITEM_SET &set = commandMngr_.historyItemSet();
	ITEM_SET::const_iterator itItem = set.begin();
	for( ; itItem != set.end(); itItem++ )
	{
		std::string msg = PaintPacketBuilder::CCreateItem::make( *itItem, target );
		allData += msg;
		itemSize += msg.size();
	}
	commandMngr_.unlock();

	// History all task
	commandMngr_.lock();
	size_t taskSize = 0;
	const TASK_ARRAY &taskList = commandMngr_.historyTaskList();
	TASK_ARRAY::const_iterator itTask = taskList.begin();
	for( ; itTask != taskList.end(); itTask++ )
	{
		std::string msg = TaskPacketBuilder::CExecuteTask::make( boost::const_pointer_cast<CSharedPaintTask>(*itTask), target );
		allData += msg;
		taskSize += msg.size();
	}
	commandMngr_.unlock();

	qDebug() << "SharedPaintManager::serializeData() size = " << allData.size() << ", task size = " << taskSize << ", item size = " << itemSize;
	return allData;
}


// this function need to check session pointer null check!
void CSharedPaintManager::dispatchPaintPacket( boost::shared_ptr<CPaintSession> session, boost::shared_ptr<CPacketData> packetData )
{
	switch( packetData->code )
	{
	case CODE_SYSTEM_JOIN:
		{
			boost::shared_ptr<CPaintUser> user = SystemPacketBuilder::CJoinerUser::parse( packetData->body );
			if( session )
				user->setSessionId( session->sessionId() );

			addUser( user );
		}
		break;
	case CODE_SYSTEM_RES_JOIN:
		{
			std::string channel;
			USER_LIST list;
			if( SystemPacketBuilder::CResponseJoin::parse( packetData->body, channel, list ) )
			{
				for( size_t i = 0; i < list.size(); i++ )
					addUser( list[i] );
			}
		}
		break;
	case CODE_SYSTEM_SYNC_START:
		{
			std::string channel, target;
			if( SystemPacketBuilder::CRequestSync::parse( packetData->body, channel, target ) )
			{
				std::string allData = serializeData( &target );
				qDebug() << "CODE_SYSTEM_SYNC_START" << allData.size() << target.c_str();

				session->session()->sendData( allData );
			}
		}
		break;
	case CODE_SYSTEM_LEFT:
		{
			std::string userId, channel;
			if( SystemPacketBuilder::CLeftUser::parse( packetData->body, channel, userId ) )
			{
				removeUser( userId );
			}
		}
		break;
	case CODE_PAINT_CLEAR_SCREEN:
		{
			PaintPacketBuilder::CClearScreen::parse( packetData->body );	// nothing to do..
			caller_.performMainThread( boost::bind( &CSharedPaintManager::fireObserver_ClearScreen, this ) );
		}
		break;
	case CODE_PAINT_CLEAR_BG:
		{
			PaintPacketBuilder::CClearScreen::parse( packetData->body );	// nothing to do..
			caller_.performMainThread( boost::bind( &CSharedPaintManager::fireObserver_ClearBackground, this ) );
		}
		break;
	case CODE_PAINT_SET_BG_IMAGE:
		{
			boost::shared_ptr<CBackgroundImageItem> image = PaintPacketBuilder::CSetBackgroundImage::parse( packetData->body );
			caller_.performMainThread( boost::bind( &CSharedPaintManager::fireObserver_SetBackgroundImage, this, image ) );
		}
		break;
	case CODE_PAINT_SET_BG_GRID_LINE:
		{
			int size;
			if( PaintPacketBuilder::CSetBackgroundGridLine::parse( packetData->body, size ) )
			{
				caller_.performMainThread( boost::bind( &CSharedPaintManager::fireObserver_SetBackgroundGridLine, this, size ) );
			}
		}
		break;
	case CODE_PAINT_SET_BG_COLOR:
		{
			int r, g, b, a;
			if( PaintPacketBuilder::CSetBackgroundColor::parse( packetData->body, r, g, b, a ) )
			{
				caller_.performMainThread( boost::bind( &CSharedPaintManager::fireObserver_SetBackgroundColor, this, r, g, b, a ) );
			}
		}
		break;	
	case CODE_PAINT_CREATE_ITEM:
		{
			std::string owner;
			int itemId;
			boost::shared_ptr<CPaintItem> item = PaintPacketBuilder::CCreateItem::parse( packetData->body );
			if( item )
			{
				commandMngr_.addHistoryItem( item );
			}
		}
		break;
	case CODE_TASK_EXECUTE:
		{
			std::string owner;
			int itemId;
			boost::shared_ptr<CSharedPaintTask> task = TaskPacketBuilder::CExecuteTask::parse( packetData->body );
			if( task )
			{
				task->setSharedPaintManager( this );

				commandMngr_.executeTask( task, false );
			}
		}
		break;
	case CODE_WINDOW_RESIZE_MAIN_WND:
		{
			std::string owner;
			int width, height;
			if( WindowPacketBuilder::CResizeMainWindow::parse( packetData->body, width, height ) )
			{
				if( width <= 0 || height <= 0 )
					return;
				caller_.performMainThread( boost::bind( &CSharedPaintManager::fireObserver_ResizeMainWindow, this, width, height ) );
			}
		}
		break;
	}
}


void CSharedPaintManager::dispatchUdpPacket( CNetUdpSession *session, boost::shared_ptr<CPacketData> packetData )
{
	switch( packetData->code )
	{
	case CODE_UDP_SERVER_INFO:
		{
			std::string addr, broadcastChannel;
			int port;
			if( UdpPacketBuilder::CServerInfo::parse( packetData->body, broadcastChannel, addr, port ) )
			{
				qDebug() << "CODE_UDP_SERVER_INFO : " << broadcastChannel.c_str() << addr.c_str() << port;
				if( broadcastChannel_ != broadcastChannel )
					return;

				caller_.performMainThread( boost::bind( &CSharedPaintManager::fireObserver_GetServerInfo, this, broadcastChannel, addr, port ) );
			}
		}
		break;
	}
}

void CSharedPaintManager::dispatchBroadCastPacket( CNetBroadCastSession *session, boost::shared_ptr<CPacketData> packetData )
{
	switch( packetData->code )
	{
	case CODE_BROAD_PROBE_SERVER:
		{
			std::string addr;
			int port;
			std::string broadcastChannel;
			if( BroadCastPacketBuilder::CProbeServer::parse( packetData->body, broadcastChannel, addr, port ) )
			{
				qDebug() << "CODE_BROAD_PROBE_SERVER : " << broadcastChannel.c_str() << addr.c_str() << port;
				if( broadcastChannel_ != broadcastChannel )
					return;

				// make server info
				std::string myIp = Util::getMyIPAddress();
				std::string broadCastMsg = UdpPacketBuilder::CServerInfo::make( broadcastChannel_, myIp, acceptPort_ );

				if( udpSessionForConnection_ )
					udpSessionForConnection_->close();

				udpSessionForConnection_ = boost::shared_ptr< CNetUdpSession >(new CNetUdpSession( netRunner_.io_service() ));
				udpSessionForConnection_->sendData( addr, port, broadCastMsg );
			}
		};

	case CODE_BROAD_TEXT_MESSAGE:
		{
			std::string message, broadcastChannel, myId;
			if( BroadCastPacketBuilder::CTextMessage::parse( packetData->body, broadcastChannel, myId, message ) )
			{
				if( broadcastChannel_ != broadcastChannel )
					return;

				if( myId_ == myId )	// ignore myself message..
					return;
	
				caller_.performMainThread( boost::bind( &CSharedPaintManager::fireObserver_ReceivedTextMessage, this, broadcastChannel, message ) );
			}
		}
		break;
	}
}
