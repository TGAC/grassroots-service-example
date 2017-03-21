# Example service

The example service is for showing developers techniques used for building a service. It shows how to run long running jobs asynchronously and how to access and query from multiple requests and processes.

## Installation

To build this service, you need the [grassroots core](https://github.com/TGAC/grassroots-core) and [grassroots build config](https://github.com/TGAC/grassroots-build-config) installed and configured. 

The files to build the SamTools service are in the ```build/<platform>``` directory. 

### Linux

If you enter this directory 

```cd build/linux```

you can then build the service by typing

```make all```

and then 

```make install```

to install the service into the Grassroots system where it will be available for use immediately.

