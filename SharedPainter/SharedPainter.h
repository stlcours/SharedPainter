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

#ifndef SHAREDPAINTER_H
#define SHAREDPAINTER_H

#ifdef Q_WS_WIN
#include <qt_windows.h>
#endif
#include <QtGui/QMainWindow>
#include "ui_sharedpainter.h"
#include "SharedPainterScene.h"
#include "SharedPaintPolicy.h"
#include "FindingServerDialog.h"
#include "SyncDataProgressDialog.h"
#include "UpgradeWindow.h"
#include "PainterListWindow.h"
#include "ScreenRecoder.h"

class SharedPainter : public QMainWindow, ICanvasViewEvent, ISharedPaintEvent, IUpgradeEvent, IScreenRecorderEvent
{
	Q_OBJECT

public:
	enum Status
	{
		INIT,
		CONNECTED,
		CONNECTFAILED,
		DISCONNECTED,
	};

	SharedPainter(CSharedPainterScene* canvas, QWidget *parent = 0, Qt::WFlags flags = 0);
	~SharedPainter();

	void setStatusBar_NetworkInfo( const std::string &addr, int listenPort )
	{
		QString str = QString(addr.c_str());
		str += ":";
		str += QString::number(listenPort);
		QString realStr = str + "  ";
		networkInfoLabel_->setText( realStr );
	}
	void setStatusBar_ConnectStatus( const QString &str )
	{
		QString realStr = str + "  ";
		statusBarLabel_->setText( realStr );
	}
	void setStatusBar_JoinerCnt( int count )
	{
		QString str = tr("  Joiner Count : ");
		str += QString::number(count);
		str += " ";
		joinerCountLabel_->setText( str );
	}
	void setStatusBar_PlaybackStatus( int current, int total )
	{
		QString str = tr("  Playback Position : ");
		str += QString::number(current);
		str += "/";
		str += QString::number(total);
		playbackStatusLabel_->setText( str );
	}
	void setStatus( Status status )
	{
		QString msg;
		switch( status )
		{
		case INIT:
			msg = tr("Not connected");
			break;
		case CONNECTED:
			msg = tr("Connected");
			break;
		case DISCONNECTED:
			msg = tr("Disconnected");
			break;
		case CONNECTFAILED:
			msg = tr("Connect Failure");
			break;
		}

		setStatusBar_ConnectStatus( msg );
		setTrayIcon( status );
	}

	void setTrayIcon( Status status )
	{
		status_ = status;

		QIcon icon;
		switch( status )
		{
		case CONNECTED:
			icon = QIcon( QPixmap(":/SharedPainter/Resources/tray_connect.png") );
			break;
		default:
			icon = QIcon( QPixmap(":/SharedPainter/Resources/tray_disconnect.png") );
			break;
		}
		trayIcon_->setIcon( icon );
		setWindowIcon( icon );
	}

	void showTrayMessage( const QString &str, const QString *strTitle = NULL)
	{
		QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information;

		if( strTitle != NULL )
			trayIcon_->showMessage( *strTitle, str, icon, DEFAULT_TRAY_MESSAGE_DURATION_MSEC );
		else
			trayIcon_->showMessage( PROGRAME_TEXT, str, icon, DEFAULT_TRAY_MESSAGE_DURATION_MSEC );
 	}

	void addTextItem( const QString &str, const QFont &font, const QColor &textColor )
	{
		boost::shared_ptr<CTextItem> textItem = boost::shared_ptr<CTextItem>(new CTextItem( str, font, textColor ));
		textItem->setMyItem();

		QPointF pos = Util::calculateNewTextPos( 
			canvas_->sceneRect().width(), canvas_->sceneRect().height(), 
			ui.painterView->mapFromGlobal(QCursor::pos()).x(),
			ui.painterView->mapFromGlobal(QCursor::pos()).y(),
			font.pixelSize(), 
			&lastTextPosX_, &lastTextPosY_ );
	
		textItem->setPos( pos.x(), pos.y() );

		requestAddItem( textItem );
	}

	void showFindingServerWindow( void )
	{
		hideFindingServerWindow();

		findingServerWindow_ = new FindingServerDialog(this);
		findingServerWindow_->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint); 
		findingServerWindow_->setWindowTitle( "Finding server.." );
		findingServerWindow_->exec();
		if( findingServerWindow_ && findingServerWindow_->isCanceled() )
		{
			SharePaintManagerPtr()->stopFindingServer();
		}
		if( findingServerWindow_ )	delete findingServerWindow_;
		findingServerWindow_ = NULL;
	}

	void hideFindingServerWindow( void )
	{
		if( findingServerWindow_ )
		{
			findingServerWindow_->reject();
		}
	}

	void showSyncProgressWindow( void )
	{
		hideSyncProgressWindow();

		syncProgressWindow_ = new SyncDataProgressDialog(this);
		syncProgressWindow_->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint); 
		syncProgressWindow_->setWindowTitle( "Sync now.." );
		syncProgressWindow_->exec();
		if( syncProgressWindow_ && syncProgressWindow_->isCanceled() )
		{
			// all session, data, status clear!!!
			SharePaintManagerPtr()->close();
		}

		if( syncProgressWindow_ )	delete syncProgressWindow_;
		syncProgressWindow_ = NULL;
	}

	void hideSyncProgressWindow( void )
	{
		if( syncProgressWindow_ )
		{
			syncProgressWindow_->reject();
		}
	}

	void onIScreenRecorderEvent_RecordStop( ScreenRecoder *self, const QString &filePath, const std::string &errormsg )
	{
		QFileInfo info(filePath);
		Util::executeProgram(info.absolutePath());
		Util::executeProgram(filePath);
	}

protected:
	void closeEvent( QCloseEvent *evt );
	void moveEvent( QMoveEvent * evt );
	void showEvent ( QShowEvent * evt );
	void resizeEvent( QResizeEvent *evt );
	void keyPressEvent( QKeyEvent *evt );
	bool eventFilter(QObject *object, QEvent *evt);
	void changeEvent ( QEvent * event )
	{
		if( event->type() == QEvent::ActivationChange )
		{
			if( isActiveWindow() )
			{
#ifdef Q_WS_WIN
				::FlashWindow( winId(), FALSE);
#endif
			}
		}
	}
	
protected slots:
	void onPaintViewVScrollBarChanged( int value );
	void onPaintViewHScrollBarChanged( int value );
	void clickedJoinerButton( void );
	void onAppSafeStarted( void );
	void splitterMoved( int pos, int index );
	void onTimer( void );
	void onTrayMessageClicked( void );
	void onTrayActivated( QSystemTrayIcon::ActivationReason reason );
	void onPlaybackSliderValueChanged( int value  );
	void applySetting( void );

	void actionAbout( void );
	void actionCloseConnection( void );
	void actionConnectServer( void );
	void actionSaveImageFile( void );
	void actionClipboardPaste( void );
	void actionNickName( void );
	void actionPaintChannel( void );
	void actionBroadcastTextMessage( void );
	void actionClearBG( void );
	void actionBGColor( void );
	void actionAddText( void );
	void actionClearScreen( void );
	void actionPenWidth( void );
	void actionPenWidth1( void );
	void actionPenWidth3( void );
	void actionPenWidth6( void );
	void actionPenWidth10( void );
	void actionPenWidth20( void );
	void actionPenColor( void );
	void actionPenMode( void );
	void actionFreePenMode( void );
	void actionMoveMode( void );
	void actionExit( void );
	void actionConnect( void );
	void actionScreenShot( void );
	void actionUndo( void );
	void actionRedo( void );
	void actionFindingServer( void );
	void actionGridLine( void );
	void actionImportFile( void );
	void actionExportFile( void );
	void actionLastItem( void );
	void actionPreferences( void );
	void actionPainterList( void );
	void actionScreenRecord( void );
	void actionScreenStreaming( void );

private:
	void setEnabledPainter( bool enabled );
	void updateWindowTitle( void );
	void updateLastChatTime( void );
	void addSystemMessage( const QString &chatMsg );
	void addYourChatMessage( const QString & userId, const QString &nickName, const QString &chatMsg );
	void addMyChatMessage( const QString & userId, const QString &nickName, const QString &chatMsg );
	void addBroadcastChatMessage(  const QString & channel, const QString & userId, const QString &nickName, const QString &chatMsg );
	void exportToFile( const std::string &data, const QString & path );
	void importFromFile( const QString & path );
	void autoExportToFile( void );
	void sendChatMessage( void );
	void requestAddItem( boost::shared_ptr<CPaintItem> item );
	void setCheckGridLineAction( bool checked );
	bool getPaintChannelString( bool force = false );
	bool getNickNameString( bool force = false );
	void changeToobarButtonColor( QPushButton *button, const QColor &clr )
	{
		QString s;
		s += "background-color: " + clr.name() + ";";
		s += "width:20; height:20;";
		button->setStyleSheet(s);
	}

	void showErrorMessage( const std::string &error )
	{
		QMessageBox::critical( this, "", Util::toStringFromUtf8(error) );
	}

	void updateRecordingStatus( void ) 
	{
		int recordingCnt = 0;
		USER_LIST list = SharePaintManagerPtr()->userList();
		for( size_t i = 0 ; i < list.size(); i++ )
		{
			if(list[i]->isScreenRecording()) 
			{
				recordingCnt++;
			}
		}

		if( recordingCnt > 0 )
			ui.recodingIndicator->show();
		else 
			ui.recodingIndicator->hide();
	}

	void checkIfItemVisibleAndRecognize( boost::shared_ptr<CPaintItem> item, const QString &msg )
	{
#define VIEW_W	ui.painterView->width()
#define VIEW_H	ui.painterView->height()

		static const int TW = 120;
		static const int TH = 50;
		static const int DIR_EAST = 0x1;
		static const int DIR_WEST = 0x2;
		static const int DIR_SOUTH = 0x4;
		static const int DIR_NORTH = 0x8;
	
		QPointF spos = ui.painterView->mapToScene(0, 0);
		QRectF rect = item->boundingRect();
		QRectF sceneRect( spos.x(), spos.y(), VIEW_W, VIEW_H );
	
		int direction = 0;
		if( (rect.x() + rect.width()) < sceneRect.x() )
			direction |= DIR_WEST;

		if( (rect.y() + rect.height()) < sceneRect.y() )
			direction |= DIR_NORTH;

		if( rect.x() > (sceneRect.x() + sceneRect.width()) )
			direction |= DIR_EAST;

		if( rect.y() > (sceneRect.y() + sceneRect.height()) )
			direction |= DIR_SOUTH;

		int posX = 0;
		int posY = 0;

		if( direction == DIR_EAST )					{ posX = VIEW_W - TW;	posY = VIEW_H/2;	}
		else if( direction == DIR_WEST )			{ posX = 0;				posY = VIEW_H/2;	}
		else if( direction == DIR_NORTH )			{ posX = VIEW_W/2;		posY = 0;			}
		else if( direction == DIR_SOUTH )			{ posX = VIEW_W/2;		posY = VIEW_H - TH;	}
		else if( direction == (DIR_EAST|DIR_NORTH) ){ posX = VIEW_W - TW;	posY = 0;			}
		else if( direction == (DIR_EAST|DIR_SOUTH) ){ posX = VIEW_W - TW;	posY = VIEW_H - TH;	}
		else if( direction == (DIR_WEST|DIR_NORTH) ){ posX = 0;				posY = 0;			}
		else if( direction == (DIR_WEST|DIR_SOUTH) ){ posX = 0;				posY = VIEW_H - TH;	}
	
		//qDebug() << "checkIfItemVisibleAndRecognize : VIEW = " << VIEW_W << VIEW_H << " ITEM = " << rect << " SCENE = " << sceneRect << " DIR = " << direction << " RES = " << posX << posY;
		if( direction != 0 )
		{
			QPoint gpos = ui.painterView->mapToGlobal( QPoint(posX, posY) );
			QToolTip::showText( gpos, msg, ui.painterView ); 
		}
	}

protected:
	// ICanvasViewEvent
	virtual void onICanvasViewEvent_MoveItem( CSharedPainterScene *view, boost::shared_ptr< CPaintItem > item );
	virtual void onICanvasViewEvent_DrawItem( CSharedPainterScene *view, boost::shared_ptr<CPaintItem> item );
	virtual void onICanvasViewEvent_UpdateItem( CSharedPainterScene *view, boost::shared_ptr<CPaintItem> item );
	virtual void onICanvasViewEvent_RemoveItem( CSharedPainterScene *view, boost::shared_ptr<CPaintItem> item );
	virtual boost::shared_ptr<CPaintItem> onICanvasViewEvent_FindItem( CSharedPainterScene *view, const std::string &owner, int itemId )
	{
		return SharePaintManagerPtr()->findPaintItem( owner, itemId );
	}
	virtual QString onICanvasViewEvent_GetToolTipText( CSharedPainterScene *view, boost::shared_ptr<CPaintItem> item );

	// ISharedPaintEvent
	virtual void onISharedPaintEvent_ShowErrorMessage( CSharedPaintManager *self, const std::string &error )
	{
		CDefferedCaller::singleShot( boost::bind(&SharedPainter::showErrorMessage, this, error) );
	}

	virtual void onISharedPaintEvent_Connected( CSharedPaintManager *self )
	{
		setStatus( CONNECTED );

		hideFindingServerWindow();

		updateRecordingStatus();
	}

	virtual void onISharedPaintEvent_ConnectFailed( CSharedPaintManager *self )
	{
		setStatus( CONNECTFAILED );
	}

	virtual void onISharedPaintEvent_Disconnected( CSharedPaintManager *self )
	{
		if( self->isFindingServerMode() )
		{
			CDefferedCaller::singleShot( boost::bind(&SharedPainter::showFindingServerWindow, this) );
		}

		setStatus( DISCONNECTED );

		hideSyncProgressWindow();

		setStatusBar_JoinerCnt( self->userCount() );

		screenRecoder_.stopStream();
		streamingUserId_ = "";
	}

	virtual void onISharedPaintEvent_ReceivedPacket( CSharedPaintManager *self )
	{
		if( isActiveWindow() == false )
		{
#ifdef Q_WS_WIN
			::FlashWindow( winId(), TRUE);
#endif
		}
		modifiedFlag_ = true;
	}
	
	virtual void onISharedPaintEvent_SyncStart( CSharedPaintManager *self )
	{
		CDefferedCaller::singleShot( boost::bind(&SharedPainter::showSyncProgressWindow, this) );
	}

	virtual void onISharedPaintEvent_SyncComplete( CSharedPaintManager *self )
	{
		CDefferedCaller::singleShot( boost::bind(&SharedPainter::hideSyncProgressWindow, this) );
	}

	virtual void onISharedPaintEvent_SendingPacket( CSharedPaintManager *self, int packetId, size_t wroteBytes, size_t totalBytes )
	{
		if( currPacketId_ != packetId )
		{
			wroteProgressBar_->setRange(0, totalBytes);
			currPacketId_ = packetId;
		}

		wroteProgressBar_->setValue( wroteBytes );

		/*
		// TODO : SENDING PACKET ITEM PROGRESS
		//qDebug() << "onISharedPaintEvent_SendingPacket" << packetId << wroteBytes << totalBytes;
		ITEM_LIST list = self->findItem( packetId );
		for( size_t i = 0; i < list.size(); i++ )
		{
			list[i]->drawSendingStatus( wroteBytes, totalBytes );
		}
		*/
	}

	virtual void onISharedPaintEvent_AddTask( CSharedPaintManager *self, int totalTaskCount, bool playBackWorking )
	{
		modifiedFlag_ = true;

		//qDebug() << "onISharedPaintEvent_AddTask" << totalTaskCount << playBackWorking;
		toolBar_SliderPlayback_->setRange( 0, totalTaskCount );
		playbackSliderFreezingFlag_ = true;
		if( ! playBackWorking )
			toolBar_SliderPlayback_->setValue( toolBar_SliderPlayback_->maximum() );
		playbackSliderFreezingFlag_ = false;

		setStatusBar_PlaybackStatus( toolBar_SliderPlayback_->value(), toolBar_SliderPlayback_->maximum() );
	}

	virtual void onISharedPaintEvent_AddPaintItem( CSharedPaintManager *self, boost::shared_ptr<CPaintItem> item )
	{
		item->setCanvas( canvas_ );
		item->draw();
	
		checkIfItemVisibleAndRecognize( item, tr("Item added") );

		if( item->type() == PT_TEXT )
		{
			lastTextPosX_ = item->posX();
			lastTextPosY_ = item->posY();
		}
	}

	virtual void onISharedPaintEvent_UpdatePaintItem( CSharedPaintManager *self, boost::shared_ptr<CPaintItem> item )
	{
		item->update();

		checkIfItemVisibleAndRecognize( item, tr("Item updated") );
	}

	virtual void onISharedPaintEvent_RemovePaintItem( CSharedPaintManager *self, boost::shared_ptr<CPaintItem> item )
	{
		checkIfItemVisibleAndRecognize( item, tr("Item removed") );

		item->remove();
	}

	virtual void onISharedPaintEvent_MovePaintItem( CSharedPaintManager *self, boost::shared_ptr<CPaintItem> item, double x, double y )
	{
		item->move( x, y );

		checkIfItemVisibleAndRecognize( item, tr("Item moved") );
	}

	virtual void onISharedPaintEvent_ClearScreen( CSharedPaintManager *self )
	{
		autoExportToFile();

		setCheckGridLineAction( false );

		// clear playback info
		playbackSliderFreezingFlag_ = true;
		toolBar_SliderPlayback_->setRange( 0, 0 );
		setStatusBar_PlaybackStatus( 0, 0 );
		playbackSliderFreezingFlag_ = false;
		
		setEnabledPainter( true );

		// thaw canvas
		canvas_->thawAction();

		modifiedFlag_ = false;
	}

	virtual void onISharedPaintEvent_SetBackgroundGridLine( CSharedPaintManager *self, int size )
	{
		modifiedFlag_ = true;

		setCheckGridLineAction( size > 0 );
		canvas_->drawBackgroundGridLine( size );
	}

	virtual void onISharedPaintEvent_SetBackgroundImage( CSharedPaintManager *self, boost::shared_ptr<CBackgroundImageItem> image ) 
	{
		modifiedFlag_ = true;

		canvas_->drawBackgroundImage( image );
	}

	virtual void onISharedPaintEvent_SetBackgroundColor( CSharedPaintManager *self, int r, int g, int b, int a )
	{
		modifiedFlag_ = true;

		canvas_->setBackgroundColor( r, g, b, a );
	}

	virtual void onISharedPaintEvent_ClearBackground( CSharedPaintManager *self )
	{
		modifiedFlag_ = true;

		setCheckGridLineAction( false );
		canvas_->drawBackgroundGridLine( 0 );
		canvas_->clearBackgroundImage();
	}

	virtual void onISharedPaintEvent_ResizeMainWindow( CSharedPaintManager *self, int width, int height )
	{
		modifiedFlag_ = true;

		if( SettingManagerPtr()->isSyncWindowSize() == false )
			return;

		resizeFreezingFlag_ = true;
		resize( width, height );
		resizeFreezingFlag_ = false;
	}

	virtual void onISharedPaintEvent_ResizeCanvas( CSharedPaintManager *self, int width, int height )
	{
		modifiedFlag_ = true;

		canvas_->setSceneRect(0, 0, width, height);
	}

	virtual void onISharedPaintEvent_ChangeCanvasScrollPos( CSharedPaintManager *self, int posH, int posV )
	{
		modifiedFlag_ = true;

		if( SettingManagerPtr()->isSyncWindowSize() == false )
			return;

		changeScrollPosFreezingFlag_ = true;

		ui.painterView->horizontalScrollBar()->setValue( posH );
		ui.painterView->verticalScrollBar()->setValue( posV );

		changeScrollPosFreezingFlag_ = false;
	}

	virtual	void onISharedPaintEvent_ResizeWindowSplitter( CSharedPaintManager *self, std::vector<int> &sizes )
	{
		modifiedFlag_ = true;

		if( SettingManagerPtr()->isSyncWindowSize() == false )
			return;

		resizeSplitterFreezingFlag_ = true;
		QList<int> list;
		for( size_t i = 0; i < sizes.size(); i++ )
			list.push_back( sizes[i] );
		ui.splitter->setSizes( list );
		resizeSplitterFreezingFlag_ = false;
	}

	virtual void onISharedPaintEvent_EnterPaintUser( CSharedPaintManager *self, boost::shared_ptr<CPaintUser> user )
	{
		QString qNick = Util::toStringFromUtf8( user->nickName() );
		QString msg = qNick + tr(" joined.");
		addSystemMessage( msg );

		setStatusBar_JoinerCnt( self->userCount() );
	}

	virtual void onISharedPaintEvent_LeavePaintUser( CSharedPaintManager *self, boost::shared_ptr<CPaintUser> user )
	{
		QString qNick = Util::toStringFromUtf8( user->nickName() );
		QString msg = qNick + tr(" left.");
		addSystemMessage( msg );

		setStatusBar_JoinerCnt( self->userCount() );

		if( user->userId() == streamingUserId_ )
			screenRecoder_.stopStream();
	}

	virtual void onISharedPaintEvent_UpdatePaintUser( CSharedPaintManager *self, boost::shared_ptr<CPaintUser> user )
	{
		setStatusBar_JoinerCnt( self->userCount() );
	}

	virtual void onISharedPaintEvent_GetServerInfo( CSharedPaintManager *self, const std::string &paintChannel, const std::string &addr, int port )
	{
		if( !self->isConnected() && self->isFindingServerMode() )
		{
			if( self->isConnecting() )
				return;

			self->connectToPeer( addr, port );
		}
	}
	
	virtual void onISharedPaintEvent_ServerFinding( CSharedPaintManager *self, int sentCount )
	{
		if( findingServerWindow_ )
		{
			int remainCount = FINDING_SERVER_TRY_COUNT - sentCount;
			if( remainCount < 0)
				findingServerWindow_->setCanceled();
			else
				findingServerWindow_->setRemainCount( remainCount );
		}
	}

	virtual void onISharedPaintEvent_ChangedNickName( CSharedPaintManager *self, const std::string & userId, const std::string &prevNickName, const std::string &currNickName )
	{
		QString qPrevNick = Util::toStringFromUtf8( prevNickName );
		QString qCurrNick = Util::toStringFromUtf8( currNickName );

		QString msg = qPrevNick + tr(" has changed a nickname to ") + qCurrNick;
		addSystemMessage( msg );
	}

	virtual void onISharedPaintEvent_ReceivedChatMessage( CSharedPaintManager *self, const std::string & userId, const std::string &nickName, const std::string &chatMsg )
	{
		QString qId = Util::toStringFromUtf8( userId );
		QString qNick = Util::toStringFromUtf8( nickName );
		QString qMsg = Util::toStringFromUtf8( chatMsg );

		if( userId == self->myId() )
			addMyChatMessage( qId, qNick, qMsg );
		else
			addYourChatMessage( qId, qNick, qMsg );

		if( isActiveWindow() == false )
		{
			if( userId != self->myId() )
				showTrayMessage( qMsg, &qNick );
		}
	}

	virtual void onISharedPaintEvent_ReceivedBroadcastTextMessage( CSharedPaintManager *self, const std::string &paintChannel, const std::string &fromId, const std::string &nickName, const std::string &message )
	{
		QString qChannel = Util::toStringFromUtf8( paintChannel );
		QString qId = Util::toStringFromUtf8( fromId );
		QString qNick = Util::toStringFromUtf8( nickName );
		QString qMsg = Util::toStringFromUtf8( message );

		addBroadcastChatMessage( qChannel, qId, qNick, qMsg );

		if( isActiveWindow() == false )
		{
			if( fromId != self->myId() )
				showTrayMessage( qMsg, &qNick );
		}
	}

	virtual void onISharedPaintEvent_ChangeScreenRecordStatus( CSharedPaintManager *self, boost::shared_ptr<CPaintUser> user, bool status )
	{
		updateRecordingStatus();
	}

	virtual void onISharedPaintEvent_ChangeShowScreenStream( CSharedPaintManager *self, boost::shared_ptr<CPaintUser> user, bool sender, bool status ) 
	{
		if( status )
		{
			if( sender )
			{
				streamingUserId_ = user->userId();

				int listenPort = self->sendResponseShowScreenStream( user->userId(), true );
				if( listenPort > 0 ) 
				{
					screenRecoder_.setWindowId( ui.painterView->winId() );
					screenRecoder_.playStream( Util::getMyIPAddress(), listenPort );	// TODO: udp stream, when use viewIPAddress??
				}
			}
			else
			{
				// TODO : not streamer, start playing stream, concurrent counter increasement
			}
		}
		else
		{
			if( sender )
			{
				streamingUserId_ = "";

				screenRecoder_.stopStream();
			}
			else
			{
				if( screenRecoder_.isStreaming() )	// i'm streamer, but other person stop stream play..
				{
					int cnt = 0;
					USER_LIST list = SharePaintManagerPtr()->userList();
					for( size_t i = 0 ; i < list.size(); i++ )
					{
						if(list[i]->isScreenStreamingReceiver()) 
						{
							cnt++;
						}
					}

					if( cnt <= 0 )	// no rececivers..  
					{
						screenRecoder_.stopStream();
						SharePaintManagerPtr()->notifyChangeShowScreenStream( true, false );
						QMessageBox::warning( this, "", tr("No stream receivers. screen streaming stopped.") );
					}
				}

				// TODO : not streamer, stop playing stream, concurrent counter decreasement
			}
		}
	}

	virtual void onIScreenRecorderEvent_ReadyStreamingData( ScreenRecoder *self, const std::string &streamData )
	{
		SharePaintManagerPtr()->sendScreenStreamData( streamData );
	}

	virtual void onIUpgradeEvent_NewVersion( CUpgradeManager *self, const std::string &version, const std::string &patchContents )
	{
		UpgradeWindow wnd(this);

		wnd.setContents( version.c_str(), Util::toStringFromUtf8( patchContents ) );

		wnd.exec();
	}



private:
	Ui::SharedPainterClass ui;

	CSharedPainterScene* canvas_;
	bool enabledPainter_;
	bool modifiedFlag_;
	int currPacketId_;
	bool changeScrollPosFreezingFlag_;
	bool resizeFreezingFlag_;
	bool resizeSplitterFreezingFlag_;
	bool playbackSliderFreezingFlag_;
	bool screenShotMode_;
	bool exitFlag_;
	QPoint orgPos_;
	QLabel *statusBarLabel_;
	QLabel *joinerCountLabel_;
	QLabel *playbackStatusLabel_;
	QLabel *networkInfoLabel_;
	QAction *penWidthAction_;
	QAction *penModeAction_;
	QAction *gridLineAction_;
	QAction *startFindServerAction_;
	QAction *toolBar_MoveMode_;
	QAction *toolBar_PenMode_;
	QAction *toolBar_GridLine_;
	QSlider *toolBar_SliderPlayback_;
	QPushButton *toolBar_penColorButton_;
	QPushButton *toolBar_bgColorButton_;
	QProgressBar *wroteProgressBar_;
	PainterListWindow *painterListWindow_;
	QTimer *keyHookTimer_;

	double lastTextPosX_;
	double lastTextPosY_;

	Status status_;
	QSystemTrayIcon *trayIcon_;
	QMenu *trayIconMenu_;
	QFont fontBroadCastText_;

	FindingServerDialog *findingServerWindow_;
	SyncDataProgressDialog *syncProgressWindow_;

	QString lastChatUserId_;

	ScreenRecoder screenRecoder_;
	std::string streamingUserId_;
};

#endif // SHAREDPAINTER_H
