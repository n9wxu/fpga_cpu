//===--- FCPU.h - fpga_cpu target info for clang ----------------*- C++ -*-===//
//
// fpga_cpu: 16-bit load/store CPU with 32-bit register-pair pointers.
// int = 16 bits; long, pointers and size_t = 32 bits; float and double are
// both IEEE single (like AVR), so only single-precision soft-float is needed.
//
// The int32 target feature (-Xclang -target-feature -Xclang +int32, which
// fcc --int32 passes) makes int 32 bits, for C written for 32-bit machines
// (Doom, CoreMark). Code built that way must be linked with a runtime built
// the same way.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_BASIC_TARGETS_FCPU_H
#define LLVM_CLANG_LIB_BASIC_TARGETS_FCPU_H

#include "clang/Basic/TargetInfo.h"
#include "clang/Basic/TargetOptions.h"
#include "llvm/Support/Compiler.h"
#include "llvm/TargetParser/Triple.h"

namespace clang {
namespace targets {

class LLVM_LIBRARY_VISIBILITY FCPUTargetInfo : public TargetInfo {
public:
  FCPUTargetInfo(const llvm::Triple &Triple, const TargetOptions &)
      : TargetInfo(Triple) {
    TLSSupported = false;
    IntWidth = 16;
    IntAlign = 16;
    LongWidth = 32;
    LongLongWidth = 64;
    LongAlign = LongLongAlign = 16;
    PointerWidth = 32;
    PointerAlign = 16;
    FloatWidth = 32;
    FloatAlign = 16;
    DoubleWidth = 32;
    DoubleAlign = 16;
    DoubleFormat = &llvm::APFloat::IEEEsingle();
    LongDoubleWidth = 32;
    LongDoubleAlign = 16;
    LongDoubleFormat = &llvm::APFloat::IEEEsingle();
    SuitableAlign = 16;
    DefaultAlignForAttributeAligned = 16;
    SizeType = UnsignedLong;
    PtrDiffType = SignedLong;
    IntPtrType = SignedLong;
    IntMaxType = SignedLongLong;
    Char16Type = UnsignedInt;
    WCharType = SignedInt;
    WIntType = SignedInt;
    Char32Type = UnsignedLong;
    SigAtomicType = SignedInt;
    resetDataLayout("e-m:e-p:32:16-i32:16-i64:16-f32:16-f64:16-a:8-n16-S16");
  }

  void getTargetDefines(const LangOptions &Opts,
                        MacroBuilder &Builder) const override;

  // scheduling models (llvm/target/FCPU/FCPUSchedule.td); fcc --cpu
  bool isValidCPUName(StringRef Name) const override {
    return Name == "generic" || Name == "fcpu1" || Name == "fcpu2";
  }
  bool setCPU(const std::string &Name) override { return isValidCPUName(Name); }

  ArrayRef<Builtin::Info> getTargetBuiltins() const override {
    return std::nullopt;
  }

  bool allowsLargerPreferedTypeAlignment() const override { return false; }

  bool hasFeature(StringRef Feature) const override {
    return Feature == "fcpu" || (Feature == "int32" && IntWidth == 32);
  }

  bool isValidFeatureName(StringRef Name) const override {
    return Name == "int32";
  }

  bool handleTargetFeatures(std::vector<std::string> &Features,
                            DiagnosticsEngine &Diags) override {
    for (const auto &F : Features)
      if (F == "+int32") {
        IntWidth = 32;
        Char16Type = UnsignedShort;
      }
    return true;
  }

  ArrayRef<const char *> getGCCRegNames() const override;

  ArrayRef<TargetInfo::GCCRegAlias> getGCCRegAliases() const override {
    static const TargetInfo::GCCRegAlias Aliases[] = {
        {{"sp"}, "p7"},
        {{"lr"}, "p6"},
    };
    return llvm::ArrayRef(Aliases);
  }

  bool validateAsmConstraint(const char *&Name,
                             TargetInfo::ConstraintInfo &Info) const override {
    switch (*Name) {
    case 'p': // a register pair
      Info.setAllowsRegister();
      return true;
    }
    return false;
  }

  std::string_view getClobbers() const override { return ""; }

  BuiltinVaListKind getBuiltinVaListKind() const override {
    return TargetInfo::CharPtrBuiltinVaList;
  }
};

} // namespace targets
} // namespace clang

#endif // LLVM_CLANG_LIB_BASIC_TARGETS_FCPU_H
