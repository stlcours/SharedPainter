#pragma once

#include "CommonPacketBuilder.h"
#include "PaintItem.h"
#include "PaintItemFactory.h"

namespace PaintPacketBuilder
{
	class CSetBackgroundGridLine
	{
	public:
		static std::string make( int size, const std::string *target = NULL )
		{		
			std::string data;
			int pos = 0;
			try
			{
				pos += CPacketBufferUtil::writeInt16( data, pos, size, true );

				return CommonPacketBuilder::makePacket( CODE_PAINT_SET_BG_GRID_LINE, data, NULL, target );
			}catch(...)
			{
			}

			return "";
		}

		static bool parse( const std::string &body, int &size )
		{		
			int pos = 0;
			try
			{
				boost::int16_t s;
				pos += CPacketBufferUtil::readInt16( body, pos, s, true );

				size = s;
				return true;
			}catch(...)
			{
			}
			return false;
		}
	};


	class CSetBackgroundColor
	{
	public:
		static std::string make( int r, int g, int b, int a, const std::string *target = NULL )
		{		
			std::string data;
			int pos = 0;
			try
			{
				pos += CPacketBufferUtil::writeInt16( data, pos, r, true );
				pos += CPacketBufferUtil::writeInt16( data, pos, g, true );
				pos += CPacketBufferUtil::writeInt16( data, pos, b, true );
				pos += CPacketBufferUtil::writeInt16( data, pos, a, true );

				return CommonPacketBuilder::makePacket( CODE_PAINT_SET_BG_COLOR, data, NULL, target );
			}catch(...)
			{
			}

			return "";
		}

		static bool parse( const std::string &body, int &r, int &g, int &b, int &a )
		{		
			int pos = 0;
			try
			{
				boost::int16_t rr, gg, bb, aa;
				pos += CPacketBufferUtil::readInt16( body, pos, rr, true );
				pos += CPacketBufferUtil::readInt16( body, pos, gg, true );
				pos += CPacketBufferUtil::readInt16( body, pos, bb, true );
				pos += CPacketBufferUtil::readInt16( body, pos, aa, true );

				r = rr;
				g = gg;
				b = bb;
				a = aa;
				return true;
			}catch(...)
			{
			}
			return false;
		}
	};

	class CSetBackgroundImage
	{
	public:
		static std::string make( boost::shared_ptr<CBackgroundImageItem> item, const std::string *target = NULL )
		{		
			std::string data = item->serialize();

			try
			{
				return CommonPacketBuilder::makePacket( CODE_PAINT_SET_BG_IMAGE, data, NULL, target );
			}catch(...)
			{

			}

			return "";
		}

		static boost::shared_ptr<CBackgroundImageItem> parse( const std::string &body )
		{		
			boost::shared_ptr< CBackgroundImageItem > item;

			try
			{
				item = boost::shared_ptr< CBackgroundImageItem >( new CBackgroundImageItem );
				if( !item->deserialize( body ) )
				{
					return boost::shared_ptr<CBackgroundImageItem>();
				}
			}catch(...)
			{
				return boost::shared_ptr<CBackgroundImageItem>();
			}

			return item;
		}
	};

	class CClearBackground
	{
	public:
		static std::string make( void )
		{		
			try
			{
				std::string body;
				return CommonPacketBuilder::makePacket( CODE_PAINT_CLEAR_BG, body );
			}catch(...)
			{

			}

			return "";
		}

		static bool parse( const std::string &body )
		{		
			try
			{
				// NOTHING BODY
				return true;
			}catch(...)
			{
				return false;
			}
			return true;
		}
	};

	class CCreateItem
	{
	public:
		static std::string make( boost::shared_ptr<CPaintItem> item, const std::string *target = NULL )
		{		
			std::string body;
			std::string data = item->serialize();

			int pos = 0;
			try
			{
				pos += CPacketBufferUtil::writeInt16( body, pos, item->type(), true );
				body += data;

				return CommonPacketBuilder::makePacket( CODE_PAINT_CREATE_ITEM, body, NULL, target );
			}catch(...)
			{
				
			}

			return "";
		}

		static boost::shared_ptr<CPaintItem> parse( const std::string &body )
		{		
			boost::shared_ptr< CPaintItem > item;

			try
			{
				int pos = 0;
				boost::int16_t temptype;
				pos += CPacketBufferUtil::readInt16( body, pos, temptype, true );

				PaintItemType type = (PaintItemType)temptype;

				item = CPaintItemFactory::createItem( type );

				std::string itemData( (const char *)body.c_str() + pos, body.size() - pos );
				if( !item->deserialize( itemData ) )
				{
					return boost::shared_ptr<CPaintItem>();
				}
			}catch(...)
			{
				return boost::shared_ptr<CPaintItem>();
			}

			return item;
		}
	};

	class CClearScreen
	{
	public:
		static std::string make( void )
		{		
			try
			{
				std::string body;
				return CommonPacketBuilder::makePacket( CODE_PAINT_CLEAR_SCREEN, body );
			}catch(...)
			{

			}

			return "";
		}

		static bool parse( const std::string &body )
		{		
			try
			{
				// NOTHING BODY
				return true;
			}catch(...)
			{
				return false;
			}
			return true;
		}
	};
};
