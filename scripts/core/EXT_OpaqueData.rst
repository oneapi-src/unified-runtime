<%
import re
from templates import helper as th
%><%
    OneApi=tags['$OneApi']
    x=tags['$x']
    X=x.upper()
%>
:orphan:

.. _${X}_OPAQUE_DATA_EXT_NAME:

=====================
Opaque Data Extension
=====================

API
---

* Macros

    * ${X}_OPAQUE_DATA_EXT_NAME


* Functions

    * ${x}PlatformGetOpaqueDataExt


Opaque Data Extension
~~~~~~~~~~~~~~~~~~~~~

Some devices whose device code is compiled by the host compiler (e.g. CPU emulators) may use 
this extension to access some device code functionality implemented in/behind the adapter.

Dependencies
~~~~~~~~~~~~

This extension has no dependencies and relies of v0.6 of the Unified Runtime specification.

Extension Changelog
~~~~~~~~~~~~~~~~~~~

v0.1
====
Document changes ...

v0.2
====
Document changes ... 


Contributors
~~~~~~~~~~~~

    * Contributor A, Company A, contrA@unified-runtime.com
    * Contributor B, Company B, contrB@unified-runtime.com
    * ...
