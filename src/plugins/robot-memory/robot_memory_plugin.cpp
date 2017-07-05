/***************************************************************************
 *  robot_memory_plugin.cpp - Fawkes Robot Memory Plugin
 *
 *  Created: Sun May 01 13:34:51 2016
 *  Copyright  2016  Frederik Zwilling
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

#include "robot_memory_thread.h"
#include "robot_memory_setup.h"
#include <string>
#include <logging/console.h>

#include <core/plugin.h>

using namespace fawkes;

/** Robot Memory Plugin.
 * This plugin provides a robot memory with mongodb
 *
 * @author Frederik Zwilling
 */
class RobotMemoryPlugin : public fawkes::Plugin
{
 public:
  /** Constructor.
   * @param config Fawkes configuration
   */
  RobotMemoryPlugin(Configuration *config) : Plugin(config)
  {
    //before starting the robot memory (thread), we have to setup the mongod and mongos processes
    //because the mongodb aspect of the robot memory hat to connect to it
    logger_for_setup = new ConsoleLogger(Logger::LL_WARN);
    setup = new RobotMemorySetup(config, logger_for_setup);
    setup->setup_mongods();

    std::string mongo_client_connection = config->get_string("plugins/robot-memory/setup/mongo-client-connection-local");
	  thread_list.push_back(new RobotMemoryThread(mongo_client_connection));
  }


  ~RobotMemoryPlugin()
  {
    delete setup;
    delete logger_for_setup;
  }

 private:
  RobotMemorySetup* setup;
  Logger *logger_for_setup;
};


PLUGIN_DESCRIPTION("Robot Memory based on MongoDB")
EXPORT_PLUGIN(RobotMemoryPlugin)
