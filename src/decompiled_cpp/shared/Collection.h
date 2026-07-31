/**
 * Collection.h — compatibility include for the canonical collection types.
 *
 * The implementations and complete layouts live in collections.h and
 * collections.cpp.  Keeping one class definition is important here: the
 * original collection views share their storage, but C++ must still have one
 * canonical type definition for each vtable-bearing class.
 */
#pragma once

#include "collections.h"
