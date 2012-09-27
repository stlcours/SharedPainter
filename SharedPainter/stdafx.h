#pragma once

#include <QtGui>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/shared_ptr.hpp>
#include <iostream>
#include <deque>

#include "DefferedCaller.h"
#include "SharedPaintPolicy.h"
#include "SharedPaintManager.h"
#include "UpgradeManager.h"
#include "SettingManager.h"
#include "NetServiceRunner.h"

extern int _debug_paint_item_cnt;