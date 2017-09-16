# PODEM
PODEM - Automatic Test Pattern Generation for combinational VLSI circuits

The algorithm is used to generate automatic test patterns for a given combinational VLSI circuit.
The generated test vector will provide full fault coverage for the circuit.

The files in the project are as follows:

main - this file performs runtime optimization on the test pattern detection by performing event driven simulation to provide upto 10x speedup.

main_tso - this file is used to perform test set optimization to reduce the size of the generated test vector while still providing the same fault coverage.
