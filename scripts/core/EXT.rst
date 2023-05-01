<%
    OneApi=tags['$OneApi']
    x=tags['$x']
    X=x.upper()
%>
==============
 Extensions
==============

Objective
=========

Extensions allow for additional functionality to be added to the Unified Runtime specification
without affecting the core specification. Extensions may in future be promoted to the core specification
in a later version of Unified Runtime, when agreed upon by the Working Group. All extensions are 
optional and are not required to be implemented by any particular adapter, but are expected to 
be widely available and possibly be required in a future version of the specification. Adapters 
must report which extensions are supported through the ${x}PlatformGetExtensionProperties query.
Each extension may also impose additional restrictions on when it can be used - i.e. a platform
or device query.

There are two types of extensions defined by this specification:

1. **Standard** -  extension will be included into the current and all future version of the specification.
2. **Experimental** - extensions require additional experimentation and development, before becoming a standard extension. 
   Applications should not rely on experimental extensions in production.

Requirements
============

- Extensions must use globally unique names for macros, enums, structures and functions
- Extensions must have globally unique extension names reported from ${x}PlatformGetExtensionProperties
- All extensions must be defined in this specification
- Extensions must not break backwards compatibility of the core APIs
- Standard extension versions must be backwards compatible with prior versions


Naming Convention
-----------------

The following naming conventions must be followed for **standard** extensions:
## --validate=off
  - The extension name must begin with `${x}_ext`
  - All extension functions must be postfixed with `Ext`
  - All macros must use all caps `${X}_<NAME>_EXT` convention
  - All structures, enumerations and other types must follow `${x}_<name>_ext_t` snake case convention
  - All enumerator values must use all caps `${X}_ENUM_EXT_ETOR_NAME` convention
  - All handle types must end with `ext_handle_t`
  - All descriptor structures must end with `ext_desc_t`
  - All property structures must end with `ext_properties_t`
  - All flag enumerations must end with `ext_flags_t`
## --validate=on

The following naming conventions must be followed for **experimental** extensions:

## --validate=off
  - The extension name must begin with `${x}_exp`
  - Experimental extensions may be added and removed from the driver at any time.
  - Experimental extensions are not guaranteed to be forward- or backward-compatible between versions.
  - Experimental extensions are not guaranteed to be supported in production driver releases; and may appear and disappear from release to release.
  - All extension functions must be postfixed with `Exp`
  - All macros must use all caps `${X}_<NAME>_EXP` convention
  - All structures, enumerations and other types must follow `${x}_<name>_exp_t` snake case convention
  - All enumerator values must use all caps `${X}_ENUM_EXP_ETOR_NAME` convention
  - All handle types must end with `exp_handle_t`
  - All descriptor structures must end with `exp_desc_t`
  - All property structures must end with `exp_properties_t`
  - All flag enumerations must end with `exp_flags_t`
## --validate=on

Extending Enumerations
----------------------

Any existing enumeration may be extended by adding new enumerators. Enumerators must use the extensions naming 
convention and values should be assigned to avoid future compatibility issues.


Extending Structures
--------------------

Any structure derived from `${x}_base_desc_t`` or `${x}_base_properties_t`` can be extended using a structure chain
by adding a pointer to the extended structure in `pNext` member variable. No other method of extending structures is allowed.

A structure chain can contain more than one extension structure, in any order. Therefore, extensions should not
be dependent on their order relative to other extensions and the implementation must be order agnostic. In addition,
the implementation will ignore extended structures that it does not support.

The extension must document the specific structures and functions that may be extended using the structure chain.


// TODO - list all extensions
