add_llvm_component_library(LLVMFCPUInfo
  FCPUTargetInfo.cpp

  LINK_COMPONENTS
  MC
  Support

  ADD_TO_COMPONENT
  FCPU
  )
