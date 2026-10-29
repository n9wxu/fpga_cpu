//===-- FCPUInstrInfo.td - fpga_cpu instructions ------------*- tablegen -*-===//
//
// One definition per instruction and addressing mode, in the syntax of
// sim/tools/asm.py. Encodings and the 16-bit short forms are the
// assembler's job, so there are no encoding fields here.
//
// Flags: arithmetic, logic and shifts write SR; ADC/SBC/CPC and Bcc read
// it. Moves, loads, stores and pair arithmetic leave it alone.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Target nodes
//===----------------------------------------------------------------------===//

def SDT_FCPUCmp      : SDTypeProfile<0, 2, [SDTCisSameAs<0, 1>]>;
def SDT_FCPUBrCC     : SDTypeProfile<0, 2, [SDTCisVT<0, OtherVT>,
                                            SDTCisVT<1, i16>]>;
def SDT_FCPUSelectCC : SDTypeProfile<1, 3, [SDTCisSameAs<0, 1>,
                                            SDTCisSameAs<1, 2>,
                                            SDTCisVT<3, i16>]>;
def SDT_FCPUWrapper  : SDTypeProfile<1, 1, [SDTCisSameAs<0, 1>,
                                            SDTCisPtrTy<0>]>;
def SDT_FCPUCall     : SDTypeProfile<0, -1, [SDTCisVT<0, i32>]>;
def SDT_FCPUCallSeqStart : SDCallSeqStart<[SDTCisVT<0, i32>,
                                           SDTCisVT<1, i32>]>;
def SDT_FCPUCallSeqEnd   : SDCallSeqEnd<[SDTCisVT<0, i32>,
                                         SDTCisVT<1, i32>]>;

def FCPUcmp      : SDNode<"FCPUISD::CMP", SDT_FCPUCmp, [SDNPOutGlue]>;
def FCPUbrcc     : SDNode<"FCPUISD::BR_CC", SDT_FCPUBrCC,
                          [SDNPHasChain, SDNPInGlue]>;
def FCPUselectcc : SDNode<"FCPUISD::SELECT_CC", SDT_FCPUSelectCC,
                          [SDNPInGlue]>;
def FCPUwrapper  : SDNode<"FCPUISD::Wrapper", SDT_FCPUWrapper>;
def FCPUcall     : SDNode<"FCPUISD::CALL", SDT_FCPUCall,
                          [SDNPHasChain, SDNPOutGlue, SDNPOptInGlue,
                           SDNPVariadic]>;
def FCPUret      : SDNode<"FCPUISD::RET_GLUE", SDTNone,
                          [SDNPHasChain, SDNPOptInGlue, SDNPVariadic]>;
def callseq_start : SDNode<"ISD::CALLSEQ_START", SDT_FCPUCallSeqStart,
                           [SDNPHasChain, SDNPOutGlue]>;
def callseq_end   : SDNode<"ISD::CALLSEQ_END", SDT_FCPUCallSeqEnd,
                           [SDNPHasChain, SDNPOptInGlue, SDNPOutGlue]>;

//===----------------------------------------------------------------------===//
// Operands
//===----------------------------------------------------------------------===//

def brtarget   : Operand<OtherVT>;
def calltarget : Operand<i32>;
def ccop : Operand<i16> { let PrintMethod = "printCC"; }

// [pN + disp]
def memd : Operand<i32> {
  let PrintMethod = "printMemD";
  let MIOperandInfo = (ops PR, i16imm);
}
// [pN + rX*s] / [pN + rX.z*s]; the immediate is scale | (zext << 3)
def memx : Operand<i32> {
  let PrintMethod = "printMemX";
  let MIOperandInfo = (ops PR, GPR, i16imm);
}
// [abs24]: a symbol (plus offset) or a constant address
def mema : Operand<i32> {
  let PrintMethod = "printMemA";
  let MIOperandInfo = (ops i32imm);
}

def addrd : ComplexPattern<i32, 2, "SelectAddrD", [frameindex], []>;
def addrx : ComplexPattern<i32, 3, "SelectAddrX", [], []>;
def addrxs : ComplexPattern<i32, 3, "SelectAddrXScaled", [add], []>;
// Tried first, so a symbol is never treated as a base register.
def addra : ComplexPattern<i32, 1, "SelectAddrA", [], [], 30>;

def simm16_i32 : PatLeaf<(i32 imm), [{ return isInt<16>(N->getSExtValue()); }]>;

def LO16 : SDNodeXForm<imm, [{
  return CurDAG->getTargetConstant(N->getZExtValue() & 0xFFFF, SDLoc(N),
                                   MVT::i16);
}]>;
def HI16 : SDNodeXForm<imm, [{
  return CurDAG->getTargetConstant((N->getZExtValue() >> 16) & 0xFFFF,
                                   SDLoc(N), MVT::i16);
}]>;

//===----------------------------------------------------------------------===//
// Instruction classes
//===----------------------------------------------------------------------===//

class FI<dag outs, dag ins, string asm, list<dag> pattern = []>
    : Instruction {
  let Namespace = "FCPU";
  dag OutOperandList = outs;
  dag InOperandList = ins;
  let AsmString = asm;
  let Pattern = pattern;
  let Size = 4;
}

class FPseudo<dag outs, dag ins, list<dag> pattern = []>
    : FI<outs, ins, "", pattern> {
  let isPseudo = 1;
  let isCodeGenOnly = 1;
}

//===----------------------------------------------------------------------===//
// ALU
//===----------------------------------------------------------------------===//

multiclass ALU3<string mn, SDNode node, bit comm = 0> {
  def rr : FI<(outs GPR:$rd), (ins GPR:$rs, GPR:$rt),
              mn # "\t$rd, $rs, $rt",
              [(set GPR:$rd, (node GPR:$rs, GPR:$rt))]> {
    let isCommutable = comm;
  }
}

multiclass ALUI<string mn, SDNode node> {
  def ri : FI<(outs GPR:$rd), (ins GPR:$rs, i16imm:$imm),
              mn # "\t$rd, $rs, $imm",
              [(set GPR:$rd, (node GPR:$rs, (i16 imm:$imm)))]>;
}

let Defs = [SR] in {
  defm ADD  : ALU3<"ADD", add, 1>;
  defm SUB  : ALU3<"SUB", sub>;
  defm AND  : ALU3<"AND", and, 1>;
  defm OR   : ALU3<"OR",  or,  1>;
  defm XOR  : ALU3<"XOR", xor, 1>;
  defm SHL  : ALU3<"SHL", shl>;
  defm SHR  : ALU3<"SHR", srl>;
  defm SAR  : ALU3<"SAR", sra>;

  defm ADDI : ALUI<"ADDI", add>;
  defm ANDI : ALUI<"ANDI", and>;
  defm ORI  : ALUI<"ORI",  or>;
  defm XORI : ALUI<"XORI", xor>;
  defm SHLI : ALUI<"SHLI", shl>;
  defm SHRI : ALUI<"SHRI", srl>;
  defm SARI : ALUI<"SARI", sra>;

  def NEG : FI<(outs GPR:$rd), (ins GPR:$rs), "NEG\t$rd, $rs",
               [(set GPR:$rd, (ineg GPR:$rs))]>;
  def NOT : FI<(outs GPR:$rd), (ins GPR:$rs), "NOT\t$rd, $rs",
               [(set GPR:$rd, (not GPR:$rs))]>;

  def CMPrr : FI<(outs), (ins GPR:$rs, GPR:$rt), "CMP\t$rs, $rt",
                 [(FCPUcmp GPR:$rs, GPR:$rt)]>;
  let isCompare = 1 in
  def CMPI : FI<(outs), (ins GPR:$rs, i16imm:$imm), "CMPI\t$rs, $imm",
                [(FCPUcmp GPR:$rs, imm:$imm)]>;
}

// Carry-chained forms, used by the 32-bit expansions.
let Defs = [SR], Uses = [SR] in {
  def ADCrr : FI<(outs GPR:$rd), (ins GPR:$rs, GPR:$rt), "ADC\t$rd, $rs, $rt">;
  def SBCrr : FI<(outs GPR:$rd), (ins GPR:$rs, GPR:$rt), "SBC\t$rd, $rs, $rt">;
  def CPCrr : FI<(outs), (ins GPR:$rs, GPR:$rt), "CPC\t$rs, $rt">;
}

// Multiplies leave the flags alone.
defm MUL   : ALU3<"MUL", mul, 1>;
defm MULH  : ALU3<"MULH", mulhs, 1>;
defm MULHU : ALU3<"MULHU", mulhu, 1>;

let isMoveReg = 1 in
def MOV : FI<(outs GPR:$rd), (ins GPR:$rs), "MOV\t$rd, $rs">;

let isMoveImm = 1, isReMaterializable = 1, isAsCheapAsAMove = 1 in
def MOVI : FI<(outs GPR:$rd), (ins i16imm:$imm), "MOVI\t$rd, $imm",
              [(set GPR:$rd, imm:$imm)]>;

//===----------------------------------------------------------------------===//
// Pairs: 32-bit values and pointers
//===----------------------------------------------------------------------===//

let isMoveReg = 1 in
def MOVP : FI<(outs PR:$pd), (ins PR:$ps), "MOVP\t$pd, $ps">;

let isCommutable = 1 in
def ADDPP : FI<(outs PR:$pd), (ins PR:$ps, PR:$pt), "ADDPP\t$pd, $ps, $pt",
               [(set PR:$pd, (add PR:$ps, PR:$pt))]>;
def ADDP : FI<(outs PR:$pd), (ins PR:$ps, GPR:$rx), "ADDP\t$pd, $ps, $rx",
              [(set PR:$pd, (add PR:$ps, (sext GPR:$rx)))]>;
def ADDPU : FI<(outs PR:$pd), (ins PR:$ps, GPR:$rx), "ADDPU\t$pd, $ps, $rx",
               [(set PR:$pd, (add PR:$ps, (zext GPR:$rx)))]>;
def ADDPI : FI<(outs PR:$pd), (ins PR:$ps, i32imm:$imm),
               "ADDPI\t$pd, $ps, $imm",
               [(set PR:$pd, (add PR:$ps, simm16_i32:$imm))]>;

def LEAd : FI<(outs PR:$pd), (ins memd:$a), "LEA\t$pd, $a">;
def LEAx : FI<(outs PR:$pd), (ins memx:$a), "LEA\t$pd, $a",
              [(set PR:$pd, addrxs:$a)]>;
let isReMaterializable = 1, isAsCheapAsAMove = 1 in
def LEAa : FI<(outs PR:$pd), (ins mema:$a), "LEA\t$pd, $a">;

def : Pat<(FCPUwrapper tglobaladdr:$a),   (LEAa tglobaladdr:$a)>;
def : Pat<(FCPUwrapper texternalsym:$a),  (LEAa texternalsym:$a)>;
def : Pat<(FCPUwrapper tjumptable:$a),    (LEAa tjumptable:$a)>;
def : Pat<(FCPUwrapper tblockaddress:$a), (LEAa tblockaddress:$a)>;
def : Pat<(FCPUwrapper tconstpool:$a),    (LEAa tconstpool:$a)>;

// 32-bit constants: two MOVIs. The assembler uses the short form for each
// half that fits.
def : Pat<(i32 imm:$i),
          (REG_SEQUENCE PR, (MOVI (LO16 imm:$i)), sub_lo,
                            (MOVI (HI16 imm:$i)), sub_hi)>;


// Width changes.
def : Pat<(i16 (trunc PR:$x)), (EXTRACT_SUBREG PR:$x, sub_lo)>;
def : Pat<(i32 (zext GPR:$x)),
          (REG_SEQUENCE PR, GPR:$x, sub_lo, (MOVI (i16 0)), sub_hi)>;
def : Pat<(i32 (sext GPR:$x)),
          (REG_SEQUENCE PR, GPR:$x, sub_lo,
                            (SARIri GPR:$x, (i16 15)), sub_hi)>;
def : Pat<(i32 (anyext GPR:$x)),
          (INSERT_SUBREG (i32 (IMPLICIT_DEF)), GPR:$x, sub_lo)>;

// A scaled 16-bit index, (sext|zext x) << k for k = 1, 2, is kept whole
// by the shift lowering so it can fold into [pN + rX*s]. Elsewhere:
// low half x << k, high half x >> (16 - k), arithmetic for sext.
foreach k = [1, 2] in {
  def : Pat<(shl (i32 (sext GPR:$x)), (i16 k)),
            (REG_SEQUENCE PR, (SHLIri GPR:$x, (i16 k)), sub_lo,
                              (SARIri GPR:$x, (i16 !sub(16, k))),
                              sub_hi)>;
  def : Pat<(shl (i32 (zext GPR:$x)), (i16 k)),
            (REG_SEQUENCE PR, (SHLIri GPR:$x, (i16 k)), sub_lo,
                              (SHRIri GPR:$x, (i16 !sub(16, k))),
                              sub_hi)>;
}

// 32-bit logic: one instruction per half.
class Split32<SDNode op, Instruction I>
    : Pat<(op PR:$a, PR:$b),
          (REG_SEQUENCE PR,
             (I (EXTRACT_SUBREG PR:$a, sub_lo),
                    (EXTRACT_SUBREG PR:$b, sub_lo)), sub_lo,
             (I (EXTRACT_SUBREG PR:$a, sub_hi),
                    (EXTRACT_SUBREG PR:$b, sub_hi)), sub_hi)>;
class Split32I<SDNode op, Instruction I>
    : Pat<(op PR:$a, imm:$i),
          (REG_SEQUENCE PR,
             (I (EXTRACT_SUBREG PR:$a, sub_lo), (LO16 imm:$i)), sub_lo,
             (I (EXTRACT_SUBREG PR:$a, sub_hi), (HI16 imm:$i)),
             sub_hi)>;
def : Split32<and, ANDrr>;
def : Split32<or,  ORrr>;
def : Split32<xor, XORrr>;
def : Split32I<and, ANDIri>;
def : Split32I<or,  ORIri>;
let AddedComplexity = 1 in
def : Pat<(not PR:$a),
          (REG_SEQUENCE PR, (NOT (EXTRACT_SUBREG PR:$a, sub_lo)), sub_lo,
                            (NOT (EXTRACT_SUBREG PR:$a, sub_hi)),
                            sub_hi)>;
def : Split32I<xor, XORIri>;

// 32-bit subtract and compare: expanded after register allocation into
// SUB/SBC and CMP/CPC, so nothing can clobber the carry in between.
let Defs = [SR] in {
  def SUB32 : FPseudo<(outs PR:$pd), (ins PR:$ps, PR:$pt),
                      [(set PR:$pd, (sub PR:$ps, PR:$pt))]>;
  def CMP32 : FPseudo<(outs), (ins PR:$ps, PR:$pt),
                      [(FCPUcmp PR:$ps, PR:$pt)]>;
}

//===----------------------------------------------------------------------===//
// Loads and stores
//===----------------------------------------------------------------------===//

multiclass LoadM<string mn, RegisterClass RC, PatFrag ld> {
  let mayLoad = 1 in {
    def d : FI<(outs RC:$rd), (ins memd:$a), mn # "\t$rd, $a",
               [(set RC:$rd, (ld addrd:$a))]>;
    def x : FI<(outs RC:$rd), (ins memx:$a), mn # "\t$rd, $a",
               [(set RC:$rd, (ld addrx:$a))]>;
    // [abs]: variant A (and B, for most ops) has no direct form, so the
    // assembler makes it LEA p0, [abs] then op [p0]: p0 is clobbered.
    let Defs = [P0] in
    def a : FI<(outs RC:$rd), (ins mema:$a), mn # "\t$rd, $a",
               [(set RC:$rd, (ld addra:$a))]>;
  }
}

multiclass StoreM<string mn, RegisterClass RC, RegisterClass RCabs, PatFrag st> {
  let mayStore = 1 in {
    def d : FI<(outs), (ins RC:$rd, memd:$a), mn # "\t$rd, $a",
               [(st RC:$rd, addrd:$a)]>;
    def x : FI<(outs), (ins RC:$rd, memx:$a), mn # "\t$rd, $a",
               [(st RC:$rd, addrx:$a)]>;
    let Defs = [P0] in
    def a : FI<(outs), (ins RCabs:$rd, mema:$a), mn # "\t$rd, $a",
               [(st RCabs:$rd, addra:$a)]>;
  }
}

defm LDW  : LoadM<"LDW",  GPR, load>;
defm LDBU : LoadM<"LDBU", GPR, zextloadi8>;
defm LDBS : LoadM<"LDBS", GPR, sextloadi8>;
defm LDP  : LoadM<"LDP",  PR,  load>;
defm STW  : StoreM<"STW", GPR, GPRnoP0, store>;
defm STB  : StoreM<"STB", GPR, GPRnoP0, truncstorei8>;
defm STP  : StoreM<"STP", PR,  PRnoP0, store>;

// Post-increment: [pN]+ steps pN by the access size afterwards. The DAG
// combiner forms these when a pointer is bumped by exactly that size after
// the access (getPostIndexedAddressParts); loads are selected in
// FCPUISelDAGToDAG.cpp, stores by these patterns.
let Constraints = "$pn = $wb" in {
  let mayLoad = 1 in {
    def LDWpi  : FI<(outs GPR:$rd, PR:$wb), (ins PR:$pn), "LDW\t$rd, [$pn]+">;
    def LDBUpi : FI<(outs GPR:$rd, PR:$wb), (ins PR:$pn), "LDBU\t$rd, [$pn]+">;
    def LDBSpi : FI<(outs GPR:$rd, PR:$wb), (ins PR:$pn), "LDBS\t$rd, [$pn]+">;
    def LDPpi  : FI<(outs PR:$rd, PR:$wb), (ins PR:$pn), "LDP\t$rd, [$pn]+">;
  }
  let mayStore = 1 in {
    def STWpi : FI<(outs PR:$wb), (ins GPR:$rd, PR:$pn), "STW\t$rd, [$pn]+",
                   [(set PR:$wb, (post_store GPR:$rd, PR:$pn, imm))]>;
    def STBpi : FI<(outs PR:$wb), (ins GPR:$rd, PR:$pn), "STB\t$rd, [$pn]+",
                   [(set PR:$wb, (post_truncsti8 GPR:$rd, PR:$pn, imm))]>;
    def STPpi : FI<(outs PR:$wb), (ins PR:$rd, PR:$pn), "STP\t$rd, [$pn]+",
                   [(set PR:$wb, (post_store PR:$rd, PR:$pn, imm))]>;
  }
}

def : Pat<(i16 (extloadi8 addrd:$a)), (LDBUd addrd:$a)>;
def : Pat<(i16 (extloadi8 addrx:$a)), (LDBUx addrx:$a)>;
def : Pat<(i16 (extloadi8 addra:$a)), (LDBUa addra:$a)>;

//===----------------------------------------------------------------------===//
// Control flow
//===----------------------------------------------------------------------===//

let isBranch = 1, isTerminator = 1, Uses = [SR] in
def Bcc : FI<(outs), (ins brtarget:$dst, ccop:$cc), "B$cc\t$dst",
             [(FCPUbrcc bb:$dst, timm:$cc)]>;

let isBranch = 1, isTerminator = 1, isBarrier = 1 in
def BRA : FI<(outs), (ins brtarget:$dst), "BRA\t$dst", [(br bb:$dst)]>;

let isBranch = 1, isTerminator = 1, isBarrier = 1, isIndirectBranch = 1 in
def JR : FI<(outs), (ins PR:$pn), "JR\t$pn", [(brind PR:$pn)]>;

let isCall = 1, Defs = [P6, SR] in {
  def JAL  : FI<(outs), (ins calltarget:$dst), "JAL\t$dst">;
  def JALR : FI<(outs), (ins PR:$pn), "JALR\t$pn", [(FCPUcall PR:$pn)]>;
}
def : Pat<(FCPUcall tglobaladdr:$dst), (JAL tglobaladdr:$dst)>;
def : Pat<(FCPUcall texternalsym:$dst), (JAL texternalsym:$dst)>;

let isReturn = 1, isTerminator = 1, isBarrier = 1 in
def RET : FI<(outs), (ins), "RET", [(FCPUret)]>;

let hasSideEffects = 1 in
def NOP : FI<(outs), (ins), "NOP">;

//===----------------------------------------------------------------------===//
// Pseudos
//===----------------------------------------------------------------------===//

let Defs = [P7], Uses = [P7] in {
  def ADJCALLSTACKDOWN : FPseudo<(outs), (ins i32imm:$amt1, i32imm:$amt2),
                                 [(callseq_start timm:$amt1, timm:$amt2)]>;
  def ADJCALLSTACKUP   : FPseudo<(outs), (ins i32imm:$amt1, i32imm:$amt2),
                                 [(callseq_end timm:$amt1, timm:$amt2)]>;
}

let usesCustomInserter = 1, Uses = [SR] in {
  def SELECT16 : FPseudo<(outs GPR:$dst), (ins GPR:$t, GPR:$f, i16imm:$cc),
                         [(set GPR:$dst,
                               (FCPUselectcc GPR:$t, GPR:$f, timm:$cc))]>;
  def SELECT32 : FPseudo<(outs PR:$dst), (ins PR:$t, PR:$f, i16imm:$cc),
                         [(set PR:$dst,
                               (FCPUselectcc PR:$t, PR:$f, timm:$cc))]>;
}
