//===-- FCPUMCAsmInfo.cpp - fpga_cpu assembly syntax ----------------------===//
//
// Output is read by sim/tools/asm.py, so keep to the directives it knows.
//
//===----------------------------------------------------------------------===//

#include "FCPUMCAsmInfo.h"
using namespace llvm;

void FCPUMCAsmInfo::anchor() {}

FCPUMCAsmInfo::FCPUMCAsmInfo(const Triple &TT) {
  CodePointerSize = 4;
  CalleeSaveStackSlotSize = 2;
  CommentString = ";";
  PrivateGlobalPrefix = ".L";
  PrivateLabelPrefix = ".L";
  Data8bitsDirective = "\t.byte\t";
  Data16bitsDirective = "\t.word\t";
  Data32bitsDirective = "\t.long\t";
  Data64bitsDirective = nullptr; // emitted as two .long
  ZeroDirective = "\t.space\t";
  AsciiDirective = "\t.ascii\t";
  AscizDirective = "\t.asciz\t";
  AlignmentIsInBytes = false;
  UsesELFSectionDirectiveForBSS = true;
  HasDotTypeDotSizeDirective = false;
  HasSingleParameterDotFile = false;
  HasIdentDirective = false;
  SupportsDebugInformation = false;
  ExceptionsType = ExceptionHandling::None;
  UseIntegratedAssembler = false;
}
