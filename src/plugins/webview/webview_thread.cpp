
/***************************************************************************
 *  webview_thread.cpp - Thread that handles web interface requests
 *
 *  Created: Mon Oct 13 17:51:31 2008 (I5 Developer's Day)
 *  Copyright  2006-2008  Tim Niemueller [www.niemueller.de]
 *
 ****************************************************************************/

/*  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  Read the full text in the LICENSE.GPL file in the doc directory.
 */

#include "webview_thread.h"
#include "static_processor.h"
#include "blackboard_processor.h"
#include "startpage_processor.h"
#include "plugins_processor.h"
#include "service_browse_handler.h"
#include "header_generator.h"
#include "footer_generator.h"

#include <core/version.h>
#include <webview/request_dispatcher.h>
#include <webview/page_reply.h>
#include <webview/server.h>
#include <webview/url_manager.h>
#include <webview/nav_manager.h>

using namespace fawkes;


/** Prefix for the WebStaticRequestProcessor. */
const char *WebviewThread::STATIC_URL_PREFIX = "/static";
/** Prefix for the WebBlackBoardRequestProcessor. */
const char *WebviewThread::BLACKBOARD_URL_PREFIX = "/blackboard";
/** Prefix for the WebPluginsRequestProcessor. */
const char *WebviewThread::PLUGINS_URL_PREFIX = "/plugins";

/** @class WebviewThread "webview_thread.h"
 * Webview Thread.
 * This thread runs the HTTP server and handles requests via the
 * WebRequestDispatcher.
 * @author Tim Niemueller
 */


/** Constructor. */
WebviewThread::WebviewThread()
  : Thread("WebviewThread", Thread::OPMODE_CONTINUOUS),
    LoggerAspect(&__cache_logger)
{
  set_prepfin_conc_loop(true);
}


WebviewThread::~WebviewThread()
{
}

void
WebviewThread::init()
{
  __cfg_port = config->get_uint("/webview/port");

  __cache_logger.clear();

  __webview_service = new NetworkService(nnresolver, "Fawkes Webview on %h",
					 "_http._tcp", __cfg_port);
  __webview_service->add_txt("fawkesver=%u.%u.%u",
			     FAWKES_VERSION_MAJOR, FAWKES_VERSION_MINOR,
			     FAWKES_VERSION_MICRO);
  __service_browse_handler = new WebviewServiceBrowseHandler(logger, __webview_service);

  __header_gen = new WebviewHeaderGenerator(webview_nav_manager);
  __footer_gen = new WebviewFooterGenerator(__service_browse_handler);

  __dispatcher = new WebRequestDispatcher(webview_url_manager,
					  __header_gen, __footer_gen);
  __webserver  = new WebServer(__cfg_port, __dispatcher, logger);

  __startpage_processor  = new WebviewStartPageRequestProcessor(&__cache_logger);
  __static_processor     = new WebviewStaticRequestProcessor(STATIC_URL_PREFIX, RESDIR"/webview", logger);
  __blackboard_processor = new WebviewBlackBoardRequestProcessor(BLACKBOARD_URL_PREFIX, blackboard);
  __plugins_processor    = new WebviewPluginsRequestProcessor(PLUGINS_URL_PREFIX, plugin_manager);
  webview_url_manager->register_baseurl("/", __startpage_processor);
  webview_url_manager->register_baseurl(STATIC_URL_PREFIX, __static_processor);
  webview_url_manager->register_baseurl(BLACKBOARD_URL_PREFIX, __blackboard_processor);
  webview_url_manager->register_baseurl(PLUGINS_URL_PREFIX, __plugins_processor);

  webview_nav_manager->add_nav_entry(BLACKBOARD_URL_PREFIX, "BlackBoard");
  webview_nav_manager->add_nav_entry(PLUGINS_URL_PREFIX, "Plugins");

  logger->log_info("WebviewThread", "Listening for HTTP connections on port %u", __cfg_port);

  service_publisher->publish_service(__webview_service);
  service_browser->watch_service("_http._tcp", __service_browse_handler);

}

void
WebviewThread::finalize()
{
  service_publisher->unpublish_service(__webview_service);
  service_browser->unwatch_service("_http._tcp", __service_browse_handler);

  webview_url_manager->unregister_baseurl("/");
  webview_url_manager->unregister_baseurl(STATIC_URL_PREFIX);
  webview_url_manager->unregister_baseurl(BLACKBOARD_URL_PREFIX);
  webview_url_manager->unregister_baseurl(PLUGINS_URL_PREFIX);

  webview_nav_manager->remove_nav_entry(BLACKBOARD_URL_PREFIX);
  webview_nav_manager->remove_nav_entry(PLUGINS_URL_PREFIX);

  delete __webserver;

  delete __webview_service;
  delete __service_browse_handler;

  delete __dispatcher;
  delete __static_processor;
  delete __blackboard_processor;
  delete __startpage_processor;
  delete __plugins_processor;
  delete __footer_gen;
  delete __header_gen;
  __dispatcher = NULL;
}


void
WebviewThread::loop()
{
  __webserver->process();
}
