# Bundle Adjustment

This directory contains the heart of mmSolver's Bundle Adjustment
solver.

- `adjust_solveFunc.h/cpp` controls the solving evaluation.
- `adjust_base.h/cpp` contains the 'solve' function, and interaction
  with Maya.
- `adjust_cminpack_*.h/cpp` contains the solver loop implementation
  when using the CMinpack library.
- `adjust_levmar_*.h/cpp` contains the solver loop implementation when
  using the LevMar library (which is deprecated, but the code has not
  yet been removed.)
- `adjust_relationships.h/cpp` evaluates the Maya DAG and tries to
  find relationships between the nodes.
- `adjust_data.h` defines data structures used for Bundle Adjustment.
- `adjust_defines.h` defines constant values used in various parts of
  mmSolver (not just in the Bundle Adjustment sub-system).