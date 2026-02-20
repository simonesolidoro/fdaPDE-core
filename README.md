<div align="center"> <h1> fdaPDE </h1>

<h5> Physics-Informed Spatial and Functional Data Analysis </h5> </div>

![test-linux-gcc](https://img.shields.io/github/actions/workflow/status/fdaPDE/fdaPDE-core/test-linux-gcc.yml?branch=stable&label=test-linux-gcc)
![test-linux-clang](https://img.shields.io/github/actions/workflow/status/fdaPDE/fdaPDE-core/test-linux-clang.yml?branch=stable&label=test-linux-clang)
![test-macos-clang](https://img.shields.io/github/actions/workflow/status/fdaPDE/fdaPDE-core/test-macos-clang.yml?branch=stable&label=test-macos-clang)

This repository contains the C++, header-only, core library system for the fdaPDE project, providing basic functionalities like a finite element solver for second-order linear elliptic boundary value problems, nonlinear unconstrained optimization algorithms, linear and non-linear system solvers, multithreading support, and more.

## Documentation
Documentation can be found on our [documentation site](https://fdapde.github.io/)

## Dependencies
fdaPDE-core is an header-only library, therefore it does not require any installation. Just make sure to have it in your include path. Neverthless, to compile code including this library you need:
* A C++20 compliant compiler. Supported versions are:
     * Linux: `gcc` 11 (or higher), `clang` 15 (or higher)
	 * macOS: `apple-clang` (the XCode version of `clang`, AppleClang 15 or higher).
* The [Eigen](https://eigen.tuxfamily.org/index.php?title=Main_Page) linear algebra library, version 3.4.0.
* The [OpenMP](https://www.openmp.org) parallel computing framework.

## Run instructions

The functionalities added in the context of the APSC project are located in the `develop` branch of the repository. In particular, a custom threadpool has been implemented in fdaPDE/src/multithreading to support shared-memory parallelization. 

The related tests are found in `test/src/test_multithreading`. Each of the tests in the subfolders contains a Makefile used to compile all the source files: the code can be compiled by running `make all`. 

For the tests contained in `Test_performance`, a shell script is provided to allow the user to run benchmarks on the code varying the number of threads used for the execution of the code. The shell script found in `Test_performance/synchro_queue` also requires the user to provide the number of elements and the number of runs to be performed when calling the script through the command line.

The results of the tests in `Test_performance` can be visualized by running the accompanying R scripts.
