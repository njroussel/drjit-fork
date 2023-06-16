/*
    base.h -- Implementation of the drjit.ArrayBase subclass

    Dr.Jit: A Just-In-Time-Compiler for Differentiable Rendering
    Copyright 2023, Realistic Graphics Lab, EPFL.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#pragma once

#include "common.h"

/// Reference to the Python ArrayBase type object
extern nb::handle array_base;

/// Create and publish the ArrayBase type object
extern void export_base(nb::module_&);

/// Is 'h' a Dr.Jit array type?
inline bool is_drjit_type(nb::handle h) {
    return PyType_IsSubtype((PyTypeObject *) h.ptr(),
                            (PyTypeObject *) array_base.ptr());
}

/// Is 'type(h)' a Dr.Jit array type?
inline bool is_drjit_array(nb::handle h) { return is_drjit_type(h.type()); }