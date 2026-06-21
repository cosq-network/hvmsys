# Guide to Understanding HVM CSV Files

---

## 1. HVM Instruction Set CSV (`hvm_instruction_set.csv`)

### 1.1 Columns Explained


| Column          | Description                                                                               |
| --------------- | ----------------------------------------------------------------------------------------- |
| **Mnemonic**    | The short name or symbol for the instruction (e.g., `ADD`, `MOV`, `LD.B`).                |
| **Opcode**      | The binary code (in hexadecimal) that represents the instruction in machine language.     |
| **Encoding**    | The emitted form of the instruction (`base32` or `escape32`).                            |
| **Format**      | The instruction format: `R`, `I`, `RI`, `B`, or `J`.                                       |
| **Operands**    | Four fields: rd, rs1, rs2, imm (unused fields shown as `-`).                             |
| **Operation**   | A concise mathematical or logical expression of what the instruction does.              |
| **Description** | A detailed human-readable explanation of the instruction's purpose and behavior.          |
| **Func**        | (Optional) A sub-code for instructions that share the same opcode but differ in function. |
| **Example**     | An illustrative assembly-language example of the instruction.                            |


---

### 1.2 Terminologies

- **Mnemonic**: A human-readable abbreviation for an instruction (e.g., `ADD` for addition).
- **Opcode**: The logical numerical code that identifies the instruction.
- **Encoding**: The actual machine encoding used to emit the instruction bytes. `base32` means a normal 32-bit word; `escape32` means the opcode is emitted with the `0xFE` escape prefix.
- **Register Operands** (`rd`, `rs1`, `rs2`):
  - `rd`: Destination register (where the result is stored).
  - `rs1`, `rs2`: Source registers (inputs to the operation).
- **Immediate Operand** (`imm`): A constant value encoded directly in the instruction.
- **Format Types**:
  - **R-type**: Register operations with func field for sub-opcodes. Format: `rd, rs1, rs2, func`
  - **I-type**: Register + immediate. Format: `rd, rs1, imm15`
  - **RI-type**: Two registers + small immediate. Format: `rd, rs1, rs2, imm10`
  - **B-type**: Branch conditions. Format: `rs1, rs2, imm15`
  - **J-type**: Jump operations. Format: `rd, offset` or just `offset`
- **Func Field**: Used to distinguish between instructions that share the same opcode (e.g., `ADD=0`, `SUB=1`, `MUL=2` at opcode 0x10).
- **Extended Opcodes**: Instructions with opcodes >= 0x80 have `Encoding=escape32` and use the `0xFE` escape prefix in the instruction stream. Format field still indicates R/I/RI/B/J but the instruction occupies 8 bytes (including padding) to maintain alignment.
- **Operand Placeholders**: Unused operands are shown as `-` (e.g., `rd, rs, -, -` for single-operand instructions).

---

## 2. HVM Register Set CSV (`hvm_register_set.csv`)

### 2.1 Columns Explained


| Column           | Description                                                                                 |
| ---------------- | ------------------------------------------------------------------------------------------- |
| **Register**     | The name of the register (e.g., `r0`, `r1`, `v0`).                                          |
| **Mnemonic**     | A short alias or description of the register's role (e.g., `zero`, `arg1`, `fp`).           |
| **Width (bits)** | The size of the register in bits (current core profile uses 64-bit GPRs).        |
| **Purpose**      | A description of the register's role in the VM (e.g., "Stack pointer" or "First argument"). |


---

### 2.2 Terminologies

- **Register**: A small storage location on the CPU/VM used to hold data temporarily during execution.
- **General-Purpose Registers** (`r0`–`r31`): Used for arithmetic, logic, and data movement.
  - `r0`: Always zero (hardwired).
  - `r1`–`r8`: Used for passing arguments and returning values.
  - `r30`: Frame pointer (optional, for debugging).
  - `r31`: Stack pointer (manages the call stack).
- **Callee-Saved Registers**: Registers that must be preserved by the called function (core profile: `r16`–`r28`).
- **Caller-Saved Registers**: Registers that the caller must assume are overwritten by the called function (core profile: `r1`–`r15`).
- **Link Register** (`r29`): Holds return address consumed by `RET`.
- **Extension Note**: Vector registers may appear only in optional extension profiles, not in current core CSV.

---

## 3. How to Use These Files

### 3.1 For Developers

- **Instruction Set CSV**: Use this as a reference when writing assemblers, disassemblers, or emulators for HVM.
- **Register Set CSV**: Use this to understand register allocation and calling conventions.

### 3.2 For Educators/Students

- **Instruction Set CSV**: Study the operation and encoding of each instruction to understand how the VM executes code.
- **Register Set CSV**: Learn the purpose of each register to understand data flow and function calls.

### 3.3 For Tooling

- **Assemblers/Disassemblers**: Use the opcode and format information to encode/decode instructions.
- **Simulators/Emulators**: Implement the behavior described in the "Operation" and "Description" columns.

---

## 4. Example: Reading an Instruction

**Example 1: Standard Instruction (32-bit)**

Suppose you encounter the following row in the `hvm_instruction_set.csv`:

| Mnemonic | Opcode | Encoding | Format | Operands            | Operation          | Description          | Func | Example |
| -------- | ------ | -------- | ------ | ------------------- | ------------------ | -------------------- | ---- | ------- |
| ADD      | 0x10   | base32   | R      | rd, rs1, rs2, -     | rd = rs1 + rs2     | Adds the contents... | 0    | add r1, r2, r3 |

- **Interpretation**: The `ADD` instruction adds the values in `rs1` and `rs2`, storing the result in `rd`.
- **Encoding**: The opcode is `0x10`, `Encoding` is `base32`, and func is `0`. Instructions like `SUB=1`, `MUL=2` share this opcode.
- **Format**: R-type with operands `rd, rs1, rs2, func`. The `-` indicates unused operand field.
- **Example**: `add r1, r2, r3` adds the value in `r2` to `r3` and stores the sum in `r1`.

**Example 2: Extended Instruction (Hardware/System)**

| Mnemonic | Opcode | Encoding | Format | Operands            | Operation          | Description          | Func | Example |
| -------- | ------ | -------- | ------ | ------------------- | ------------------ | -------------------- | ---- | ------- |
| SYSCALL  | 0xC0   | escape32 | I      | rd, -, imm15        | rd = os_syscall(imm)| Triggers a system... | -    | syscall r1, r0, 10 |

- **Interpretation**: The `SYSCALL` instruction triggers an OS-level service.
- **Encoding**: Opcode `0xC0` uses `Encoding=escape32` and therefore the `0xFE` escape prefix.
- **Format**: I-type with operands `rd, rs1, imm15`. In the instruction stream, it occupies 8 bytes.
- **Example**: `syscall r1, r0, 10` invokes system call ID 10 and stores the result in `r1`.

---

## 5. Example: Reading a Register

Suppose you encounter the following row in the `hvm_register_set.csv`:


| Register | Mnemonic | Width (bits) | Purpose                          |
| -------- | -------- | ------------ | -------------------------------- |
| r1       | arg1/ret | 64           | First argument and return value. |


- **Interpretation**: `r1` is a 64-bit register used to pass the first argument to a function and to return values.

---

## 6. Summary

- Use the **Instruction Set CSV** to understand how each instruction is encoded and what it does.
- Use the **Register Set CSV** to understand the role and usage of each register in the HVM architecture.
- Refer to the terminologies section for clarity on technical terms.
