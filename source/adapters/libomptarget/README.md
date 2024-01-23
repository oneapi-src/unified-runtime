# libomptarget adapter
This adapter is a very rough proof-of-concept which was implemented to carry out
gap analysis between Unified Runtime and the libomptarget plugin interfaces.
There is enough functionality to run some basic SYCL programs of certain work
sizes.

There were a lot of gaps found and generally these have been documented in the
adapter code. The adapter is hard-coded to use the CUDA libomptarget plugin,
and as libomptarget lacks a way to programmatically query information about the
context and device, we have hard-coded it to use CUDA directly for these
features. If we did not do this then no SYCL application would be able to work.


## Building

An LLVM build with the `openmp` project enabled is required. This adapter was
developed in August 2023 and as such will likely not work with the most up to
date LLVM. To check out a known good version:
```
git clone https://github.com/llvm/llvm-project/
git checkout af35be5
```

Once LLVM with OpenMP is built, the adapter can be enabled:

```
cmake .. -DUR_BUILD_ADAPTER_LIBOMPTARGET=ON\
         -DUR_LIBOMPTARGET_LLVM_DIR=<llvm install dir>\
         -DUR_LIBOMPTARGET_INCLUDES_DIR=<llvm source dir>/openmp/libomptarget/include
```

* `UR_LIBOMPTARGET_LLVM_DIR` should point to the install directory of your LLVM build

* `UR_LIBOMPTARGET_INCLUDES_DIR` should point to the directory containing `omptargetplugin.h` in the LLVM source