# Example of Linux Daemon

This repository contains simple example of daemon for Linux OS.
This repository also contains examples of starting scripts.

## Requirements

To build example of the daemon you have to have following tools

* CMake
* GCC/CLang

## Build

To build example of daemon you have to type following commands:

    git clone https://github.com/jirihnidek/daemon.git
    cd daemon
    mkdir build
    cd build
    cmake -DCMAKE_INSTALL_PREFIX=/usr ../
    make
    sudo make install

## Usage

You can test running daemon from command line:

    ./bin/daemon

But running the app in this way is not running running daemon. Let
have a look at command line parameters and arguments

    Usage: ./bin/daemon [OPTIONS]

     Options:
      -h --help                 Print this help
      -d --daemon               Daemonize this application
      -p --pid_file  filename   PID file used by daemonized app
      -o --output_delay delay   Delay on when to display the notification

When you will run `./bin/daemon` with parameter `--daemon` or `-d`, then
it will become real UNIX daemon. But this is not the way, how UNIX daemons
are started nowdays. Some init scripts or service files are used for
this purpose.
