<%
    OneApi=tags['$OneApi']
    x=tags['$x']
    X=x.upper()
%>

.. _experimental-launch-config:

================================================================================
LAUNCH CONFIG
================================================================================

.. warning::

    Experimental features:

    *   May be replaced, updated, or removed at any time.
    *   Do not require maintaining API/ABI stability of their own additions over
        time.
    *   Do not require conformance testing of their own additions.


Terminology
--------------------------------------------------------------------------------
TODO

Motivation
--------------------------------------------------------------------------------
Advances in hardware sometimes require new kernel properties. Additionally some
applications require specification of kernel properties at runtime. This
extension is a future proof solution to support these two requirements. instead
of using a fixed set of kernel arguments, the approach is to introduce the
objects "exp_launch_config_handle_t" and "exp_launch_attr_handle_t" that enable a more
flexible approach. A launch_config_handle_t contains a set of
exp_launch_attr_handle_t that describe different kernel launch attributes.
Three new functions are introduced. "urEnqueueKernelLaunchCustomExp" takes a
launch_config_handle_t as an argument, and launches a kernel using these
properties. "urEnqueueKernelLaunchCustomExp" corresponds to the CUDA Driver API
"cuLaunchKernelEx". "urKernelSetLaunchConfigExp" and "urKernelSetLaunchAttrExp"
are designed to be backend agnostic abstractions for setting backend specific
settings for "exp_launch_config_handle_t" and "exp_launch_attr_handle_t" respectively.

Currently this extension may only supported on the CUDA backend, and the initial
implementation will be minimal; supporting only specific priority custom kernel
properties in the cuda backend (distributed shared memory within a "cluster"
of thread blocks).
Other kernel properties can be supported, such as cooperative kernels. As such,
eventually this extension should be able to replace the cooperative kernels
UR extension. 

API
--------------------------------------------------------------------------------

Macros
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* ${X}_LAUNCH_CONFIG_EXTENSION_STRING_EXP

Enums
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* ${x}_exp_launch_attr_t

Functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* ${x}KernelSetLaunchAttrExp
* ${x}KernelSetLaunchConfigExp
* ${x}EnqueueKernelLaunchCustomExp

Support
--------------------------------------------------------------------------------

Adapters which support this experimental feature *must* return the valid string
defined in ``${X}_LAUNCH_CONFIG_EXTENSION_STRING_EXP`` as one of the options from
${x}DeviceGetInfo when querying for ${X}_DEVICE_INFO_EXTENSIONS.

Changelog
--------------------------------------------------------------------------------

+-----------+---------------------------------------------+
| Revision  | Changes                                     |
+===========+=============================================+
| 1.0       | Initial Draft                               |
+-----------+---------------------------------------------+

Contributors
--------------------------------------------------------------------------------

* JackAKirk `jack.kirk@codeplay.com <jack.kirk@codeplay.com>`_
