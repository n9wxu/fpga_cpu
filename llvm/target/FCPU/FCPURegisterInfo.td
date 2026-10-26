//===-- FCPURegisterInfo.td - fpga_cpu registers ------------*- tablegen -*-===//
//
// r0-r15 are 16 bits. pN is the pair r(2N+1):r(2N), low half in the even
// register. Reserved: p0 (assembler/frame scratch), p7 (sp).
// ABI: r2-r7 arguments/results and caller-saved; r8-r11 callee-saved;
// p6 = lr (r12:r13), written by JAL/JALR, otherwise caller-saved;
// p5 = frame pointer when one is needed.
//
//===----------------------------------------------------------------------===//

let Namespace = "FCPU" in {
def sub_lo : SubRegIndex<16>;
def sub_hi : SubRegIndex<16, 16>;
}

class FReg<bits<16> num, string n> : Register<n> {
  let Namespace = "FCPU";
  let HWEncoding = num;
  let DwarfNumbers = [num];
}

class FPair<bits<16> num, string n, list<Register> subs>
    : RegisterWithSubRegs<n, subs> {
  let Namespace = "FCPU";
  let HWEncoding = num;
  let SubRegIndices = [sub_lo, sub_hi];
  let CoveredBySubRegs = 1;
  let DwarfNumbers = [!add(num, 16)];
}

foreach i = 0...15 in
  def R#i : FReg<i, "r"#i>;

foreach i = 0...7 in
  def P#i : FPair<i, "p"#i, [!cast<Register>("R"#!mul(i, 2)),
                             !cast<Register>("R"#!add(!mul(i, 2), 1))]>;

def SR : FReg<0, "sr"> { let DwarfNumbers = [24]; }

// Allocation order: caller-saved first, then callee-saved, then lr.
def GPR : RegisterClass<"FCPU", [i16], 16,
    (add R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13,
         R0, R1, R14, R15)>;

def PR : RegisterClass<"FCPU", [i32], 16,
    (add P1, P2, P3, P4, P5, P6, P0, P7)>;

// SR (the flags) is deliberately in no register class: an implicit def
// with a known type would give every flag-setting instruction a second
// result in the selection DAG.
