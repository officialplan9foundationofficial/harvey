TEXT _FORKPGRP(SB), 1, $0
MOVW R7, 0(FP)
MOVW $10, R7
TA R0
RETURN
