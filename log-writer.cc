#include <pthread.h>
#include <iostream>

#include "global.h"
#include "log-writer.h"

namespace ShardFree
{

LogWriter::LogWriter(const std::string &publisher_name) :
  mPublisherName(publisher_name),
  mPublisherp(NULL)
{
  // Create a worker thread that listens to the logger socket and prints the output
  pthread_t worker;

  zmq::socket_t ready_socket(*gZMQContextp, ZMQ_PULL);
  // FIXME: This should be uniquely named
  ready_socket.bind("inproc://writerready");

  pthread_create (&worker, NULL, runWorker, this);
  zmq::message_t message;

  // Waits until ZMQ sockets are abound before returning.
  ready_socket.recv(&message);
}

LogWriter::~LogWriter()
{
  delete mPublisherp;
  mPublisherp = NULL;
}

void LogWriter::run()
{
  mPublisherp = new zmq::socket_t(*gZMQContextp, ZMQ_SUB);
  mPublisherp->setsockopt(ZMQ_SUBSCRIBE, "", 0);

  bool connected = false;
  while (!connected)
  {
    try
    {
      mPublisherp->connect(mPublisherName.c_str());
      connected = true;
    }
    catch(...)
    {
      std::cerr << "Errno:" << errno;
      switch(errno)
      {
        case EINVAL:
          std::cerr << "EINVAL";
          break;
        case EPROTONOSUPPORT:
          std::cerr << "EPROTONOSUPPORT";
          break;
        case ENOCOMPATPROTO:
          std::cerr << "ENOCOMPATPROTO";
          break;
        case ETERM:
          std::cerr << "ETERM";
          break;
        case ENOTSOCK:
          std::cerr << "ENOTSOCK";
          break;
        case EMTHREAD:
          std::cerr << "EMTHREAD";
          break;
        default:
          std::cerr << "UNKNOWN ERROR";
      }
      std::cerr << std::endl;
      sleep(1);
    }
  }


  // Now that we're bound, tell the main thread that we're ready for use
  {
    zmq::socket_t sender(*gZMQContextp, ZMQ_PUSH);
    sender.connect("inproc://writerready");
    zmq::message_t message;
    sender.send(message);
  }

  while (1) {
    zmq::message_t message;
    mPublisherp->recv(&message);

    // FIXME: This is totally not safe and likely to break.
    // PubSub socket, which will then allow me to send it to a websocket proxy
    std::cout << std::string((char *)message.data(), message.size());
    // FIXME: Should terminate on shutdown message from parent
  }
}

void *LogWriter::runWorker(void *argp)
{
  auto log_writerp = (LogWriter *)argp;
  log_writerp->run();
  return 0;
}

}
