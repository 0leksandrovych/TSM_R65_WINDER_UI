#pragma once

#include <stdbool.h>

/* Run all protocol codec self-tests.
 * Returns true if every test passes, false on the first failure.
 * Produces no side-effects outside this compilation unit.
 * Not called automatically — invoke explicitly during factory test or startup diagnostics. */
bool winder_link_protocol_selftest(void);
