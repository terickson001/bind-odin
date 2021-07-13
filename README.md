**DEPRECATED, PLEASE USE [bind-odin-port](https://github.com/terickson001/bind-odin-port) INSTEAD**
# bind-odin
A bindings generator for [Odin](https://github.com/odin-lang/odin)

## How to use

Clone the repo:
```
$ git clone https:/github.com/terickson001/bind-odin
```

The repo comes with a `build.bat` for windows, but if you want to build using visual studio, or on another platform, you'll want to download [Premake5](https://premake.github.io/download.html#v5).

To create the build files for you're platform, run:
```
$ premake5 {platform}
```
you can use `premake5 --help` for a list of targets.

Once you've built the program, you can run `./bind --help` for a list of command line options.

Currently, all the options aren't available through the command line. For a comprehensive list and explanation of all the options, look at the example config file, `example.bind`.
