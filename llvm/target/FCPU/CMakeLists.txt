add_llvm_component_group(FCPU)

set(LLVM_TARGET_DEFINITIONS FCPU.td)

tablegen(LLVM FCPUGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM FCPUGenCallingConv.inc -gen-callingconv)
tablegen(LLVM FCPUGenDAGISel.inc -gen-dag-isel)
tablegen(LLVM FCPUGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM FCPUGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM FCPUGenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(FCPUCommonTableGen)

add_llvm_target(FCPUCodeGen
  FCPUAsmPrinter.cpp
  FCPUFrameLowering.cpp
  FCPUInstrInfo.cpp
  FCPUISelDAGToDAG.cpp
  FCPUISelLowering.cpp
  FCPUMCInstLower.cpp
  FCPURegisterInfo.cpp
  FCPUSubtarget.cpp
  FCPUTargetMachine.cpp

  LINK_COMPONENTS
  AsmPrinter
  CodeGen
  CodeGenTypes
  Core
  FCPUDesc
  FCPUInfo
  MC
  SelectionDAG
  Support
  Target
  TargetParser

  ADD_TO_COMPONENT
  FCPU
  )

add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)
