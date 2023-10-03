<%
    OneApi=tags['$OneApi']
    x=tags['$x']
    X=x.upper()
%>

.. _experimental-multi-device-compile:

================================================================================
Multi Device Compile
================================================================================

.. warning::

    Experimental features:

    *   May be replaced, updated, or removed at any time.
    *   Do not require maintaining API/ABI stability of their own additions over
        time.
    *   Do not require conformance testing of their own additions.



Motivation
--------------------------------------------------------------------------------

Instead of relying on the list of devices used to create a context, provide
interfaces which instead take a list of devices. This more closely aligns with
PI and OpenCL. Introduced to workaround a regression. May be superseded in
future.

API
--------------------------------------------------------------------------------
.. comment:
    In this section you *must* list all additions your experimental feature will 
    make to the Unified Runtime specification. If your experimental feature does 
    not include additions from one or more of the sections listed below, you may 
    freely remove them.

Functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* ${x}ProgramBuildExp
* ${x}ProgramCompileExp
* ${x}ProgramLinkExp

Changelog
--------------------------------------------------------------------------------
.. comment:
    When making a change to an experimental feature, increment the version and 
    provide a brief description of the change in the table below.

+-----------+------------------------+
| Revision  | Changes                |
+===========+========================+
| 1.0       | Initial Draft           |
+-----------+------------------------+

Support
--------------------------------------------------------------------------------

Adapters which support this experimental feature *must* return the valid string 
defined in ``${X}_MULTI_DEVICE_COMPILE_EXTENSION_STRING_EXP`` 
as one of the options from ${x}DeviceGetInfo when querying for 
${X}_DEVICE_INFO_EXTENSIONS. Conversely, before using any of the 
functionality defined in this experimental feature the user *must* use the 
device query to determine if the adapter supports this feature.

Contributors
--------------------------------------------------------------------------------
.. comment:
    Please list all people who wish to be credited for contribution to this 
    experimental feature.

* Kenneth Benzie (Benie) `k.benzie@codeplay.com <k.benzie@codeplay.com>`_
