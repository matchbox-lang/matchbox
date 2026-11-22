#ifndef ANALYZER_RECOVERY_H
#define ANALYZER_RECOVERY_H

#include "compiler/analyzer/analyzer.h"

void rebuild_analyzer(Analyzer* analyzer, const Vector* submission_boundaries, Vector* functions);

#endif
