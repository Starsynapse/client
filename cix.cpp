// $Id: cix.cpp,v 1.7 2019-02-07 15:14:37-08 - - $

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;

#include <libgen.h>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>

#include "protocol.h"
#include "logstream.h"
#include "sockets.h"

using deleter = void (*) (void*);

logstream log (cout);
struct cix_exit: public exception {};

unordered_map<string,cix_command> command_map {
   {"exit", cix_command::EXIT},
   {"help", cix_command::HELP},
   {"ls"  , cix_command::LS  },
   {"get" , cix_command::GET }
};

static const string help = R"||(
exit         - Exit the program.  Equivalent to EOF.
get filename - Copy remote file to local host.
help         - Print help summary.
ls           - List names of files on remote server.
put filename - Copy local file to remote host.
rm filename  - Remove file from remote server.
)||";

void cix_help() {
   cout << help;
}

void cix_ls (client_socket& server) {
   cix_header header;
   header.command = cix_command::LS;
   log << "sending header " << header << endl;
   send_packet (server, &header, sizeof header);
   recv_packet (server, &header, sizeof header);
   log << "received header " << header << endl;
   if (header.command != cix_command::LSOUT) {
      log << "sent LS, server did not return LSOUT" << endl;
      log << "server returned " << header << endl;
   }else {
      auto buffer = make_unique<char[]> (header.nbytes + 1);
      recv_packet (server, buffer.get(), header.nbytes);
      log << "received " << header.nbytes << " bytes" << endl;
      buffer[header.nbytes] = '\0';
      cout << buffer.get();
   }
}

void cix_get(client_socket& server, string filename)
{
    cix_header header;
    header.command = cix_command::GET;
    memcpy(header.filename, filename.c_str(), filename.size());
    header.nbytes = 0;
    log << "sending header " << header << endl;
    send_packet (server, &header, sizeof header);
    recv_packet (server, &header, sizeof header);
    log << "received header " << header << endl;
    if (header.command != cix_command::FILEOUT)
    {
        log << "sent GET, server did not return FILEOUT" << endl;
        log << "server returned " << header << endl;
    }
    else
    {
        if (!strcmp(filename.c_str(), header.filename))
        {
            log << "filename mismatch" << endl;
        }
        else
        {
            unique_ptr<char, deleter> buffer { new char[header.nbytes], free};
            recv_packet(server, buffer.get(), header.nbytes);
            log << "received" << header.nbytes << " bytes" << endl;
            ofstream file(filename);
            file.write(buffer.get(), header.nbytes);
            log << "wrote to file" << endl;
            file.close();
        }
    }
}


void usage() {
   cerr << "Usage: " << log.execname() << " [host] [port]" << endl;
   throw cix_exit();
}

int main (int argc, char** argv) {
   log.execname (basename (argv[0]));
   log << "starting" << endl;
   vector<string> args (&argv[1], &argv[argc]);
   if (args.size() > 2) usage();
   string host = get_cix_server_host (args, 0);
   in_port_t port = get_cix_server_port (args, 1);
   log << to_string (hostinfo()) << endl;
   try {
      log << "connecting to " << host << " port " << port << endl;
      client_socket server (host, port);
      log << "connected to " << to_string (server) << endl;
      for (;;) {
         string line, filename = "", command = "";
         getline (cin, line);
         auto index_to_the_first_space_ya = line.find_first_of(" ");
         if (index_to_the_first_space_ya == string::npos)
         {
             command = line;
         }
         else
         {
             command = line.substr(0,index_to_the_first_space_ya);
         }
         if (cin.eof()) throw cix_exit();
         getline(cin, filename);
         log << "command " << command << endl;
         const auto& itor = command_map.find (command);
         cix_command cmd = itor == command_map.end()
                         ? cix_command::ERROR : itor->second;
         switch (cmd) {
            case cix_command::EXIT:
               throw cix_exit();
               break;
            case cix_command::HELP:
               cix_help();
               break;
            case cix_command::LS:
               cix_ls (server);
               break;
            case cix_command::GET:
               if (index_to_the_first_space_ya == string::npos)
               {
                   log << "" << endl;
               }
               else
               {
                   filename = line.substr(index_to_the_first_space_ya);
                   cix_get(server, filename);
               }
            default:
               log << command << ": invalid command" << endl;
               break;
         }
      }
   }catch (socket_error& error) {
      log << error.what() << endl;
   }catch (cix_exit& error) {
      log << "caught cix_exit" << endl;
   }
   log << "finishing" << endl;
   return 0;
}
