add_llvm_component_library(LLVMFCPUDesc
  FCPUInstPrinter.cpp
  FCPUMCAsmInfo.cpp
  FCPUMCTargetDesc.cpp

  LINK_COMPONENTS
  MC
  FCPUInfo
  Support
  TargetParser

  ADD_TO_COMPONENT
  FCPU
  )
