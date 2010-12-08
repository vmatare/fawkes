
/***************************************************************************
 *  mongodb_logger_thread.cpp - MongoDB logger thread
 *
 *  Created: Tue Dec 07 22:59:47 2010
 *  Copyright  2006-2010  Tim Niemueller [www.niemueller.de]
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

#include "mongodb_logger_thread.h"

#include <core/threading/mutex.h>
#include <core/threading/mutex_locker.h>

// from MongoDB
#include <mongo/client/dbclient.h>

using namespace mongo;
using namespace fawkes;

/** @class MongoDBLoggerThread "mongodb_logger_thread.h"
 * Thread that provides a logger writing to MongoDB.
 * This thread provides a logger, which writes log information to a
 * MongoDB collection.
 * @author Tim Niemueller
 */

/** Constructor. */
MongoDBLoggerThread::MongoDBLoggerThread()
  : Thread("MongoDBLoggerThread", Thread::OPMODE_WAITFORWAKEUP),
    LoggerAspect(this)
{
  __mutex = new Mutex();
}


/** Destructor. */
MongoDBLoggerThread::~MongoDBLoggerThread()
{
  delete __mutex;
}


void
MongoDBLoggerThread::init()
{
  __collection = "fawkes.msglog";
  try {
    __collection = config->get_string("/plugins/mongodb/logger_collection");
  } catch (Exception &e) {}
}


void
MongoDBLoggerThread::finalize()
{
}


void
MongoDBLoggerThread::loop()
{
}


void
MongoDBLoggerThread::insert_message(LogLevel ll, const char *component,
				    const char *format, va_list va)
{
  if (log_level <= ll ) {
    MutexLocker lock(__mutex);
    struct timeval now;
    gettimeofday(&now, NULL);
    Date_t nowd = now.tv_sec * 1000 + now.tv_usec / 1000;

    char *msg;
    if (vasprintf(&msg, format, va) == -1) {
      // Cannot do anything useful, drop log message
      return;
    }

    BSONObjBuilder b;
    switch (ll) {
    case LL_DEBUG: b.append("level", "DEBUG"); break;
    case LL_INFO:  b.append("level", "INFO");  break;
    case LL_WARN:  b.append("level", "WARN");  break;
    case LL_ERROR: b.append("level", "ERROR"); break;
    default:       b.append("level", "UNKN");  break;
    }
    b.append("component", component);
    b.appendDate("time", nowd);
    b.append("message", msg);

    free(msg);

    mongodb_client->insert(__collection, b.obj());
  }
}

void
MongoDBLoggerThread::insert_message(LogLevel ll, const char *component,
				    Exception &e)
{
  if (log_level <= ll ) {
    MutexLocker lock(__mutex);
    struct timeval now;
    gettimeofday(&now, NULL);
    Date_t nowd = now.tv_sec * 1000 + now.tv_usec / 1000;

    for (Exception::iterator i = e.begin(); i != e.end(); ++i) {
      BSONObjBuilder b;
      switch (ll) {
      case LL_DEBUG: b.append("level", "DEBUG"); break;
      case LL_INFO:  b.append("level", "INFO");  break;
      case LL_WARN:  b.append("level", "WARN");  break;
      case LL_ERROR: b.append("level", "ERROR"); break;
      default:       b.append("level", "UNKN");  break;
      }
      b.append("component", component);
      b.appendDate("time", nowd);
      b.append("message", std::string("[EXCEPTION] ") + *i);
      mongodb_client->insert(__collection, b.obj());
    }
  }
}

void
MongoDBLoggerThread::vlog_debug(const char *component,
				const char *format, va_list va)
{
  insert_message(LL_DEBUG, component, format, va);
}

void
MongoDBLoggerThread::vlog_info(const char *component,
			       const char *format, va_list va)
{
  insert_message(LL_INFO, component, format, va);
}

void
MongoDBLoggerThread::vlog_warn(const char *component,
			       const char *format, va_list va)
{
  insert_message(LL_WARN, component, format, va);
}

void
MongoDBLoggerThread::vlog_error(const char *component,
				const char *format, va_list va)
{
  insert_message(LL_ERROR, component, format, va);
}

void
MongoDBLoggerThread::log_debug(const char *component, const char *format, ...)
{
  va_list arg;
  va_start(arg, format);
  insert_message(LL_DEBUG, component, format, arg);
  va_end(arg);
}

void
MongoDBLoggerThread::log_info(const char *component, const char *format, ...)
{
  va_list arg;
  va_start(arg, format);
  insert_message(LL_INFO, component, format, arg);
  va_end(arg);
}

void
MongoDBLoggerThread::log_warn(const char *component, const char *format, ...)
{
  va_list arg;
  va_start(arg, format);
  insert_message(LL_WARN, component, format, arg);
  va_end(arg);
}

void
MongoDBLoggerThread::log_error(const char *component, const char *format, ...)
{
  va_list arg;
  va_start(arg, format);
  insert_message(LL_ERROR, component, format, arg);
  va_end(arg);
}

void
MongoDBLoggerThread::log_debug(const char *component, Exception &e)
{
  insert_message(LL_DEBUG, component, e);
}

void
MongoDBLoggerThread::log_info(const char *component, Exception &e)
{
  insert_message(LL_INFO, component, e);
}

void
MongoDBLoggerThread::log_warn(const char *component, Exception &e)
{
  insert_message(LL_WARN, component, e);
}

void
MongoDBLoggerThread::log_error(const char *component, Exception &e)
{
  insert_message(LL_ERROR, component, e);
}

void
MongoDBLoggerThread::tlog_insert_message(LogLevel ll, struct timeval *t,
					 const char *component,
					 const char *format, va_list va)
{
  if (log_level <= ll ) {
    MutexLocker lock(__mutex);
    char *msg;
    if (vasprintf(&msg, format, va) == -1) {
      return;
    }

    Date_t nowd = t->tv_sec * 1000 + t->tv_usec / 1000;

    BSONObjBuilder b;
    switch (ll) {
    case LL_DEBUG: b.append("level", "DEBUG"); break;
    case LL_INFO:  b.append("level", "INFO");  break;
    case LL_WARN:  b.append("level", "WARN");  break;
    case LL_ERROR: b.append("level", "ERROR"); break;
    default:       b.append("level", "UNKN");  break;
    }
    b.append("component", component);
    b.appendDate("time", nowd);
    b.append("message", msg);
    mongodb_client->insert(__collection, b.obj());

    free(msg);

    __mutex->unlock();
  }
}

void
MongoDBLoggerThread::tlog_insert_message(LogLevel ll, struct timeval *t,
					 const char *component, Exception &e)
{
  if (log_level <= ll ) {
    MutexLocker lock(__mutex);
    Date_t nowd = t->tv_sec * 1000 + t->tv_usec / 1000;
    for (Exception::iterator i = e.begin(); i != e.end(); ++i) {
      BSONObjBuilder b;
      switch (ll) {
      case LL_DEBUG: b.append("level", "DEBUG"); break;
      case LL_INFO:  b.append("level", "INFO");  break;
      case LL_WARN:  b.append("level", "WARN");  break;
      case LL_ERROR: b.append("level", "ERROR"); break;
      default:       b.append("level", "UNKN");  break;
      }
      b.append("component", component);
      b.appendDate("time", nowd);
      b.append("message", std::string("[EXCEPTION] ") + *i);
      mongodb_client->insert(__collection, b.obj());
    }
  }
}

void
MongoDBLoggerThread::tlog_debug(struct timeval *t, const char *component,
				const char *format, ...)
{
  va_list arg;
  va_start(arg, format);
  tlog_insert_message(LL_DEBUG, t, component, format, arg);
  va_end(arg);
}

void
MongoDBLoggerThread::tlog_info(struct timeval *t, const char *component,
			       const char *format, ...)
{
  va_list arg;
  va_start(arg, format);
  tlog_insert_message(LL_INFO, t, component, format, arg);
  va_end(arg);
}

void
MongoDBLoggerThread::tlog_warn(struct timeval *t, const char *component,
			       const char *format, ...)
{
  va_list arg;
  va_start(arg, format);
  tlog_insert_message(LL_WARN, t, component, format, arg);
  va_end(arg);
}

void
MongoDBLoggerThread::tlog_error(struct timeval *t, const char *component,
				const char *format, ...)
{
  va_list arg;
  va_start(arg, format);
  tlog_insert_message(LL_ERROR, t, component, format, arg);
  va_end(arg);
}

void
MongoDBLoggerThread::tlog_debug(struct timeval *t, const char *component,
				Exception &e)
{
  tlog_insert_message(LL_DEBUG, t, component, e);
}

void
MongoDBLoggerThread::tlog_info(struct timeval *t, const char *component,
			       Exception &e)
{
  tlog_insert_message(LL_INFO, t, component, e);
}

void
MongoDBLoggerThread::tlog_warn(struct timeval *t, const char *component,
			       Exception &e)
{
  tlog_insert_message(LL_WARN, t, component, e);
}

void
MongoDBLoggerThread::tlog_error(struct timeval *t, const char *component,
				Exception &e)
{
  tlog_insert_message(LL_ERROR, t, component, e);
}

void
MongoDBLoggerThread::vtlog_debug(struct timeval *t, const char *component,
				 const char *format, va_list va)
{
  tlog_insert_message(LL_DEBUG, t, component, format, va);
}

void
MongoDBLoggerThread::vtlog_info(struct timeval *t, const char *component,
				const char *format, va_list va)
{
  tlog_insert_message(LL_INFO, t, component, format, va);
}

void
MongoDBLoggerThread::vtlog_warn(struct timeval *t, const char *component,
				const char *format, va_list va)
{
  tlog_insert_message(LL_WARN, t, component, format, va);
}

void
MongoDBLoggerThread::vtlog_error(struct timeval *t, const char *component,
				 const char *format, va_list va)
{
  tlog_insert_message(LL_ERROR, t, component, format, va);
}
