
/***************************************************************************
 *  bblogger_replay_plugin.cpp - Fawkes BlackBoard Logger Replay Plugin
 *
 *  Created: Mi Feb 17 01:53:00 2010
 *  Copyright  2010  Masrur Doostdar, Tim Niemueller [www.niemueller.de]
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

#include "bblogger_replay_plugin.h"
#include "log_replay_thread.h"

#include <utils/time/time.h>

#include <set>

#include <cstring>
#include <cerrno>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace fawkes;

/** @class BlackBoardLoggerReplayPlugin "bblogger_plugin.h"
 * BlackBoard logger replay plugin.
 * This plugin replay one or more logfiles into interface of the local blackboard
 *
 * @author Masrur Doostdar
 * @author Tim Niemueller
 */

/** Constructor.
 * @param config Fawkes configuration
 */
BlackBoardLoggerReplayPlugin::BlackBoardLoggerReplayPlugin(Configuration *config)
  : Plugin(config)
{
  std::set<std::string> ifaces;

  std::string prefix = "/fawkes/bblogger_replay/";

  std::string scenario = "";
  try {
    scenario = config->get_string((prefix + "scenario").c_str());
  } catch (Exception &e) {
    e.append("No scenario defined, configure %sscenario", prefix.c_str());
    throw;
  }

  std::string scenario_prefix = prefix + scenario + "/";
  std::string log_prefix   = scenario_prefix + "log/";

  std::string logdir = LOGDIR;
  try {
    logdir = config->get_string((scenario_prefix + "logdir").c_str());
  } catch (Exception &e) { /* ignored, use default set above */ }

  struct stat s;
  int err = stat(logdir.c_str(), &s);
  if (err != 0) {
    char buf[1024];
    Exception se ("Cannot access logdir %s (%s)",
		  logdir.c_str(), strerror_r(errno, buf, 1024));
  } else if ( ! S_ISDIR(s.st_mode) ) {
    throw Exception("Logdir path %s is not a directory", logdir.c_str());
  }

  Configuration::ValueIterator *i = config->search(log_prefix.c_str());
  while (i->next()) {
    
    //printf("Adding sync thread for peer %s\n", peer.c_str());
    BBLoggerReplayThread *log_replay_thread = new BBLoggerReplayThread(i->get_string().c_str(),
								       logdir.c_str(),
								       scenario.c_str());
    thread_list.push_back(log_replay_thread);
  }
  delete i;

  if ( thread_list.empty() ) {
    throw Exception("No interfaces configured for logging, aborting");
  }
}

PLUGIN_DESCRIPTION("Replay logfiles by writing them to BlackBoard interfaces")
EXPORT_PLUGIN(BlackBoardLoggerReplayPlugin)
