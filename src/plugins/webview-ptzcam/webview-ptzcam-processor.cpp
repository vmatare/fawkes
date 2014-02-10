
/***************************************************************************
 *  webview-ptzcam-processor.cpp - Pan/tilt/zoom camera control for webview
 *
 *  Created: Fri Feb 07 17:53:07 2014
 *  Copyright  2006-2014  Tim Niemueller [www.niemueller.de]
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

#include "webview-ptzcam-processor.h"

#include <core/exception.h>
#include <core/threading/mutex_locker.h>
#include <logging/logger.h>
#include <webview/page_reply.h>
#include <webview/error_reply.h>
#include <webview/redirect_reply.h>
#include <blackboard/blackboard.h>
#include <interfaces/PanTiltInterface.h>
#include <interfaces/CameraControlInterface.h>
#include <interfaces/SwitchInterface.h>

#include <cstring>
#include <cmath>

using namespace fawkes;

/** @class WebviewPtzCamRequestProcessor "webview-ptzcam-processor.h"
 * Pan/tilt/zoom camera request processor.
 * @author Tim Niemueller
 */


/** Constructor.
 * @param logger logger to report problems
 * @param baseurl base URL of the Clips webrequest processor
 */
WebviewPtzCamRequestProcessor::WebviewPtzCamRequestProcessor(std::string base_url, std::string image_id,
							     std::string pantilt_id, std::string camctrl_id,
							     std::string power_id,
							     float pan_increment, float tilt_increment,
							     unsigned int zoom_increment, float post_powerup_time,
							     fawkes::BlackBoard *blackboard,
							     fawkes::Logger *logger)
{
  logger_            = logger;
  blackboard_        = blackboard;
  baseurl_           = base_url;
  image_id_          = image_id;
  pan_increment_     = pan_increment;
  tilt_increment_    = tilt_increment;
  zoom_increment_    = zoom_increment;
  post_powerup_time_ = (long int)roundf(fabs(post_powerup_time) * 1000000);

  ptu_if_         = blackboard->open_for_reading<PanTiltInterface>(pantilt_id.c_str());
  camctrl_if_     = blackboard->open_for_reading<CameraControlInterface>(camctrl_id.c_str());
  power_if_       = blackboard->open_for_reading<SwitchInterface>(power_id.c_str());
}


/** Destructor. */
WebviewPtzCamRequestProcessor::~WebviewPtzCamRequestProcessor()
{
  blackboard_->close(ptu_if_);
  blackboard_->close(camctrl_if_);
  blackboard_->close(power_if_);
}


WebReply *
WebviewPtzCamRequestProcessor::process_request(const fawkes::WebRequest *request)
{
  if ( request->url().find(baseurl_) == 0 ) {
    std::string subpath = request->url().substr(baseurl_.length());

    power_if_->read();
    if (! power_if_->is_enabled()) {
      if (power_if_->has_writer()) {
	try {
	  power_if_->msgq_enqueue(new SwitchInterface::EnableSwitchMessage());
	  usleep(post_powerup_time_);
	} catch (Exception &e) {
	  logger_->log_warn("WebviewPtzCamReqProc", "Failed to power up camera, exception follows");
	  logger_->log_warn("WebviewPtzCamReqProc", e);
	}
      }
    }

    if (subpath == "/ping") {
      StaticWebReply *r = new StaticWebReply(WebReply::HTTP_OK);
      r->add_header("Content-type", "text/plain");
      r->append_body("OK\n");
      return r;
    } else if (subpath == "/move" || subpath == "/move/") {
      ptu_if_->read();
      camctrl_if_->read();

      // NOTE: this it at the moment mirrored for ceiling mounting!

      float pan_val = ptu_if_->pan(), tilt_val = ptu_if_->tilt();
      float zoom = std::max(1u, camctrl_if_->zoom());
      std::string pan_str = request->get_value("pan"), tilt_str = request->get_value("tilt");
      if (pan_str != "") {
	if (pan_str == "right") {
	  pan_val = std::max(ptu_if_->min_pan(), ptu_if_->pan() - pan_increment_ / zoom);
	} else if (pan_str == "left") {
	  pan_val = std::min(ptu_if_->max_pan(), ptu_if_->pan() + pan_increment_ / zoom);
	} else {
	  try {
	    pan_val  = std::stof(request->get_value("pan").c_str());
	  } catch (std::exception &e) {} // ignored, use current val
	}
      }
      if (tilt_str != "") {
	if (tilt_str == "up") {
	  tilt_val = std::max(ptu_if_->min_tilt(), ptu_if_->tilt() - tilt_increment_ / zoom);
	} else if (tilt_str == "down") {
	  tilt_val = std::min(ptu_if_->max_tilt(), ptu_if_->tilt() + tilt_increment_ / zoom);
	} else {
	  try {
	    tilt_val  = std::stof(request->get_value("tilt").c_str());
	  } catch (std::exception &e) {} // ignored, use current val
	}
      }

      PanTiltInterface::GotoMessage *gotomsg =
	new PanTiltInterface::GotoMessage(pan_val, tilt_val);
      ptu_if_->msgq_enqueue(gotomsg);

      StaticWebReply *r = new StaticWebReply(WebReply::HTTP_OK);
      r->add_header("Content-type", "text/plain");
      r->append_body("OK PAN %f TILT %f\n", pan_val, tilt_val);
      //r->append_body("FAIL DISABLED\n");
      return r;

    } else if (subpath == "/zoom" || subpath == "/zoom/") {
      camctrl_if_->read();

      unsigned int zoom_val = camctrl_if_->zoom();
      std::string zoom_str = request->get_value("zoom");
      if (zoom_str != "") {
	if (zoom_str == "out") {
	  zoom_val = std::max((long int)camctrl_if_->zoom_min(), (long int)camctrl_if_->zoom() - zoom_increment_);
	} else if (zoom_str == "in") {
	  zoom_val = std::min((long int)camctrl_if_->zoom_max(), (long int)camctrl_if_->zoom() + zoom_increment_);
	} else {
	  try {
	    zoom_val  = std::stol(request->get_value("zoom").c_str());
	  } catch (std::exception &e) {} // ignored, use current val
	}
      }

      CameraControlInterface::SetZoomMessage *setmsg =
	new CameraControlInterface::SetZoomMessage(zoom_val);
      camctrl_if_->msgq_enqueue(setmsg);

      StaticWebReply *r = new StaticWebReply(WebReply::HTTP_OK);
      r->add_header("Content-type", "text/plain");
      r->append_body("OK ZOOM %u\n", zoom_val);
      return r;
      
    } else if (subpath == "/effect" || subpath == "/effect/") {
      camctrl_if_->read();

      CameraControlInterface::SetEffectMessage *setmsg =
	new CameraControlInterface::SetEffectMessage();

      std::string effect_str = request->get_value("set");
      if (effect_str == "none") {
	setmsg->set_effect(CameraControlInterface::EFF_NONE);
      } else if (effect_str == "negative") {
	setmsg->set_effect(CameraControlInterface::EFF_NEGATIVE);
      } else if (effect_str == "pastel") {
	setmsg->set_effect(CameraControlInterface::EFF_PASTEL);
      } else if (effect_str == "bw") {
	setmsg->set_effect(CameraControlInterface::EFF_BW);
      } else if (effect_str == "solarize") {
	setmsg->set_effect(CameraControlInterface::EFF_SOLARIZE);
      } else {
	StaticWebReply *r = new StaticWebReply(WebReply::HTTP_OK);
	r->add_header("Content-type", "text/plain");
	r->append_body("FAIL UNKNOWN EFFECT %s\n", effect_str.c_str());
	return r;
      }

      camctrl_if_->msgq_enqueue(setmsg);

      StaticWebReply *r = new StaticWebReply(WebReply::HTTP_OK);
      r->add_header("Content-type", "text/plain");
      r->append_body("OK EFFECT %s\n", effect_str.c_str());
      return r;
      
    } else if (subpath == "" || subpath == "/") {
      WebPageReply *r = new WebPageReply("SkyCam");
      r->set_html_header(
        "  <link type=\"text/css\" href=\"/static/css/jqtheme/jquery-ui.custom.css\" rel=\"stylesheet\" />\n"
	"  <link type=\"text/css\" href=\"/static/css/webview-ptzcam.css\" rel=\"stylesheet\" />\n"
	"  <script type=\"text/javascript\" src=\"/static/js/jquery.min.js\"></script>\n"
	"  <script type=\"text/javascript\" src=\"/static/js/jquery-ui.custom.min.js\"></script>\n");

      *r += "<h2>SkyCam</h2>\n";

      r->append_body("<p><img id=\"image\" src=\"/images/view/%s.jpg\" /></p>\n", image_id_.c_str());

      // hardcoded baseurl here because it's so much simpler...
      *r +=
	"<script>\n"
	"var frame_number = 0;\n"
	"var move_jqxhr = null;\n"
	"$(function() {\n"
	"  $( \"#toggle-stream\" ).button({\n"
	"    icons: {\n"
	"      primary: \"ui-icon-play\"\n"
	"    },\n"
	"    text: false\n"
	"  })\n"
	"  .click(function() {\n"
	"    $(this).blur();\n"
	"    var options;\n"
	"    var src = $('#image').attr('src');\n"
	"    var pos = src.indexOf('?');\n"
	"    if(pos != -1)  src = src.substring(0, pos);\n"
	"    var srcstem = src.substring(0, src.lastIndexOf('.'));\n"
	"    if ( $( this ).text() === \"play\" ) {\n"
	"      options = {\n"
	"        label: \"pause\",\n"
	"        icons: {\n"
	"          primary: \"ui-icon-pause\"\n"
	"        }\n"
	"      };\n"
	"      $('#image').attr('src', srcstem + '.mjpeg');\n"
	"      $.ajax(\"/ptzcam/ping\");\n"
	"    } else {\n"
	"      options = {\n"
	"        label: \"play\",\n"
	"        icons: {\n"
	"          primary: \"ui-icon-play\"\n"
	"        }\n"
	"      };\n"
	"      frame_number += 1;\n"
	"      $('#image').attr('src', srcstem + '.jpg?' + frame_number);\n"
	"    }\n"
	"    $( this ).button( \"option\", options );\n"
	"  });\n"
	"  $( \"#refresh\" ).button({\n"
	"    icons: {\n"
	"      primary: \"ui-icon-refresh\"\n"
	"    },\n"
	"    text: false\n"
	"  })\n"
	"  .click(function() {\n"
	"    $(this).blur();\n"
	"    var src = $('#image').attr('src');\n"
	"    // check for existing ? and remove if found\n"
	"    var pos = src.indexOf('?');\n"
	"    if(pos != -1)  src = src.substring(0, pos);\n"
	"    frame_number += 1;\n"
	"    $('#image').attr('src', src + '?' + frame_number);\n"
	"    $.ajax(\"/ptzcam/ping\");\n"
	"    return false;\n"
	"  });\n"
	"  $( \"#left\" ).button({\n"
	"    icons: {\n"
	"      primary: \"ui-icon-arrowthick-1-w\"\n"
	"    },\n"
	"    text: false\n"
	"  })\n"
	"  .click(function() {\n"
	"    $(this).blur();\n"
	"    if (move_jqxhr != null)  move_jqxhr.abort();\n"
	"    move_jqxhr = $.ajax(\"/ptzcam/move?pan=left\");\n"
	"  });\n"
	"  $( \"#right\" ).button({\n"
	"    icons: {\n"
	"      primary: \"ui-icon-arrowthick-1-e\"\n"
	"    },\n"
	"    text: false\n"
	"  })\n"
	"  .click(function() {\n"
	"    $(this).blur();\n"
	"    if (move_jqxhr != null)  move_jqxhr.abort();\n"
	"    move_jqxhr = $.ajax(\"/ptzcam/move?pan=right\");\n"
	"  });\n"
	"  $( \"#up\" ).button({\n"
	"    icons: {\n"
	"      primary: \"ui-icon-arrowthick-1-n\"\n"
	"    },\n"
	"    text: false\n"
	"  })\n"
	"  .click(function() {\n"
	"    $(this).blur();\n"
	"    if (move_jqxhr != null)  move_jqxhr.abort();\n"
	"    move_jqxhr = $.ajax(\"/ptzcam/move?tilt=up\");\n"
	"  });\n"
	"  $( \"#down\" ).button({\n"
	"    icons: {\n"
	"      primary: \"ui-icon-arrowthick-1-s\"\n"
	"    },\n"
	"    text: false\n"
	"  })\n"
	"  .click(function() {\n"
	"    $(this).blur();\n"
	"    if (move_jqxhr != null)  move_jqxhr.abort();\n"
	"    move_jqxhr = $.ajax(\"/ptzcam/move?tilt=down\");\n"
	"  });\n"
	"  $( \"#center\" ).button({\n"
	"    icons: {\n"
	"      primary: \"ui-icon-bullet\"\n"
	"    },\n"
	"    text: false\n"
	"  })\n"
	"  .click(function() {\n"
	"    $(this).blur();\n"
	"    if (move_jqxhr != null)  move_jqxhr.abort();\n"
	"    move_jqxhr = $.ajax(\"/ptzcam/move?pan=0&tilt=0\");\n"
	"  });\n"
	"  $( \"#zoom-in\" ).button({\n"
	"    icons: {\n"
	"      primary: \"ui-icon-zoomin\"\n"
	"    },\n"
	"    text: false\n"
	"  })\n"
	"  .click(function() {\n"
	"    $(this).blur();\n"
	"    if (move_jqxhr != null)  move_jqxhr.abort();\n"
	"    move_jqxhr = $.ajax(\"/ptzcam/zoom?zoom=in\");\n"
	"  });\n"
	"  $( \"#zoom-out\" ).button({\n"
	"    icons: {\n"
	"      primary: \"ui-icon-zoomout\"\n"
	"    },\n"
	"    text: false\n"
	"  })\n"
	"  .click(function() {\n"
	"    $(this).blur();\n"
	"    if (move_jqxhr != null)  move_jqxhr.abort();\n"
	"    move_jqxhr = $.ajax(\"/ptzcam/zoom?zoom=out\");\n"
	"  });\n"
	"  $( \"#zoom-reset\" ).button({\n"
	"    icons: {\n"
	"      primary: \"ui-icon-search\"\n"
	"    },\n"
	"    text: false\n"
	"  })\n"
	"  .click(function() {\n"
	"    $(this).blur();\n"
	"    if (move_jqxhr != null)  move_jqxhr.abort();\n"
	"    move_jqxhr = $.ajax(\"/ptzcam/zoom?zoom=0\");\n"
	"  });\n"
	"});\n"
	"</script>\n"
	"\n"
	"<button id=\"refresh\" title=\"Refresh\">Refresh</button>\n"
	"<button id=\"toggle-stream\" title=\"Toggle Stream\">play</button>\n"
	"<button id=\"left\" title=\"Pan left\">left</button>\n"
	"<button id=\"right\" title=\"Pan right\">right</button>\n"
	"<button id=\"up\" title=\"Tilt up\">up</button>\n"
	"<button id=\"down\" title=\"Tilt down\">down</button>\n"
	"<button id=\"center\" title=\"Re-center camera\">center</button>\n"
	"<button id=\"zoom-in\" title=\"Zoom in\">zoom-out</button>\n"
	"<button id=\"zoom-out\" title=\"Zoom out\">zoom-in</button>\n"
	"<button id=\"zoom-reset\" title=\"Reset zoom\">zoom-reset</button>\n"
	"<br/>\n"
	"<form style=\"margin-top: 1em;\">\n"
	"  <div id=\"filter\">\n"
	"    <input type=\"radio\" id=\"filter-title\" name=\"radio\" />"
	"<label for=\"filter-title\">Filter</label>\n"
	"    <input type=\"radio\" id=\"filter-none\" name=\"radio\" checked=\"checked\"/>"
	"<label for=\"filter-none\">None</label>\n"
	"    <input type=\"radio\" id=\"filter-negative\" name=\"radio\" />"
	"<label for=\"filter-negative\">Negative</label>\n"
	"    <input type=\"radio\" id=\"filter-bw\" name=\"radio\" />"
	"<label for=\"filter-bw\">Black/White</label>\n"
	"    <input type=\"radio\" id=\"filter-solarize\" name=\"radio\" />"
	"<label for=\"filter-solarize\">Solarize</label>\n"
	"    <input type=\"radio\" id=\"filter-pastel\" name=\"radio\" />"
	"<label for=\"filter-pastel\">Pastel</label>\n"
	"  </div>\n"
	"</form>\n"
	"<script>\n"
	"var effect_jqxhr = null;\n"
	"$(function() {\n"
	"  $('#filter').buttonset();\n"
	"  $('#filter-title').button('option', 'disabled', true );\n"
	"  $('#filter-none').click(function() {\n"
	"    if (effect_jqxhr != null)  effect_jqxhr.abort();\n"
	"    effect_jqxhr = $.ajax(\"/ptzcam/effect?set=none\");\n"
	"  });\n"
	"  $('#filter-negative').click(function() {\n"
	"    if (effect_jqxhr != null)  effect_jqxhr.abort();\n"
	"    effect_jqxhr = $.ajax(\"/ptzcam/effect?set=negative\");\n"
	"  });\n"
	"  $('#filter-bw').click(function() {\n"
	"    if (effect_jqxhr != null)  effect_jqxhr.abort();\n"
	"    effect_jqxhr = $.ajax(\"/ptzcam/effect?set=bw\");\n"
	"  });\n"
	"  $('#filter-solarize').click(function() {\n"
	"    if (effect_jqxhr != null)  effect_jqxhr.abort();\n"
	"    effect_jqxhr = $.ajax(\"/ptzcam/effect?set=solarize\");\n"
	"  });\n"
	"  $('#filter-pastel').click(function() {\n"
	"    if (effect_jqxhr != null)  effect_jqxhr.abort();\n"
	"    effect_jqxhr = $.ajax(\"/ptzcam/effect?set=pastel\");\n"
	"  });\n"
	"});\n"
	"</script>\n";

      return r;
    } else {
      return new WebErrorPageReply(WebReply::HTTP_NOT_FOUND, "Unknown request");
    }
  } else {
    return NULL;
  }
}
