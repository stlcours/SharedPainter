#pragma once 

#define VERSION_TEXT	"0.85"
#define AUTHOR_TEXT		"gunoodaddy"
#define PROGRAME_TEXT	"Shared Painter"

#define NET_MAGIC_CODE	0xBEBE

#define DEFAULT_TEXT_ITEM_POS_REGION_W	9999
#define DEFAULT_TEXT_ITEM_POS_REGION_H	300

#define DEFAULT_PIXMAP_ITEM_SIZE_W	250

#define DEFAULT_TRAY_MESSAGE_DURATION_MSEC	5000

#define FINDING_SERVER_TRY_COUNT	20

#if defined(WINDOWS)
#define NATIVE_NEWLINE_STR	"\r\n"
#else
#define NATIVE_NEWLINE_STR	"\n"
#endif
