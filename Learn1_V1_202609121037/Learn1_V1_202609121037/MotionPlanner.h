#pragma once

#include <string>
#include <vector>
#include "GCodeModalMachine.h"

class MotionPlanner
{
public:
    bool ProcessLine(const std::vector<GCodeModalMachine::Word>& words,
        const GCodeModalMachine::ModalState& modal,
        std::vector<GCodeModalMachine::CodeError>& errors,
        std::vector<GCodeModalMachine::MotionSegment>& outSegments);
private:

};