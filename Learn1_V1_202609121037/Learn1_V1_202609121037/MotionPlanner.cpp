#include "pch.h"
#include "MotionPlanner.h"
/*****************************************
* 考虑废弃，具体功能转移到ModalMachine中实现
*****************************************/

bool MotionPlanner::ProcessLine(const std::vector<GCodeModalMachine::Word>& words,
                                const GCodeModalMachine::ModalState& modal,
                                std::vector<GCodeModalMachine::CodeError>& errors,
                                std::vector<GCodeModalMachine::MotionSegment>& outSegments)
{
    switch (modal.motion)
    {
        case GCodeModalMachine::MotionType::Rapid:
        {
            break;
        }
        case GCodeModalMachine::MotionType::Line:
        {
            break;
        }
        case GCodeModalMachine::MotionType::ArcCW:
        {
            break;
        }
        case GCodeModalMachine::MotionType::ArcCCW:
        {
            break;
        }
    }
}