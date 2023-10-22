# LambaDEX
Cartesi DApp example for the hackathon

# Build

This Makefile helps you manage Docker images and their dependencies. It provides the following targets:

```
make all                 - Build all targets (DEFAULT)
make emulator            - Build the 'emulator' image.
make manager             - Build the 'manager' image.
make sdk                 - Build the 'sdk' image.
make node                - Build the 'node' image.
make dapp                - Build the cartesi-machine dapp.
make run                 - Build the run the dapp.
make help                - Display this usage information.
```

# Run simulation

All the scripts you may need for simulation are located in the [scripts](script) folder. The entry point is the `execute_simulation.sh` script and it should be called from the same directory as `sunodo run` (e.g. from the dapp directory):

```bash
$ ./execute_simulation.sh
```

To be able to run the simulation scripts you have to install:
- [foundry](https://book.getfoundry.sh/getting-started/installation);
- [wagyu](https://github.com/howardwu/wagyu#2-build-guide).

The simulation process involves token contracts deployment. You have to install openzeppelin package for this (the command should be performed in the root project directory, the same where this README is located):

```bash
$ npm i @openzeppelin/contracts
```
