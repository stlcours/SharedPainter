#pragma once 

#define VERSION_TEXT	"0.2"
#define AUTHOR_TEXT		"gunoodaddy"

#define NET_MAGIC_CODE	0xBE

#define DEFAULT_TEXT_ITEM_POS_REGION_W	9999
#define DEFAULT_TEXT_ITEM_POS_REGION_H	300

#define DEFAULT_PIXMAP_ITEM_SIZE_W	250

enum SharedPaintCodeType {
	CODE_PAINT_ADD_ITEM,
	CODE_PAINT_UPDATE_ITEM,
	CODE_PAINT_SET_BG_IMAGE,
	CODE_PAINT_CLEAR_BG_IMAGE,
	CODE_PAINT_MOVE_ITEM,
	CODE_PAINT_REMOVE_ITEM,
	CODE_PAINT_CLEAR_SCREEN,
	CODE_WINDOW_RESIZE_MAIN_WND,
	CODE_BROAD_SERVER_INFO,
	CODE_MAX,
};