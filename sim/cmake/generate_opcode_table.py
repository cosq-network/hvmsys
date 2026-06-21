#!/usr/bin/env python3
"""Generate HVM opcode table header from instruction set CSV."""

import csv
import re
import sys


def parse_csv(path):
    rows = []
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(row)
    return rows


def sanitize_enum(name):
    name = name.upper()
    name = name.replace(".", "_")
    name = name.replace("-", "_")
    return name


def make_mnemonic_enum(rows):
    lines = ["enum class HvmMnemonic {"]
    for r in rows:
        enum_name = sanitize_enum(r["Mnemonic"])
        comment = f" // {r['Mnemonic']}: {r['Description']}"
        lines.append(f"  k{enum_name},{comment}")
    lines.append("};")
    return "\n".join(lines)


def make_inst_table(rows):
    lines = [
        "struct HvmInstInfo {",
        "  HvmMnemonic mnemonic;",
        "  uint32_t opcode;",
        "  const char* encoding;",
        "  const char* format;",
        "  const char* operands;",
        "  int func;",
        "  const char* mnemonic_str;",
        "};",
        "",
        "constexpr HvmInstInfo kHvmInstTable[] = {",
    ]

    for r in rows:
        enum_name = sanitize_enum(r["Mnemonic"])
        func = r["Func"] if r["Func"] and r["Func"] != "-" else "-1"
        lines.append(
            f'  {{ HvmMnemonic::k{enum_name}, {r["Opcode"]}, '
            f'"{r["Encoding"]}", "{r["Format"]}", '
            f'"{r["Operands"]}", {func}, '
            f'"{r["Mnemonic"]}" }},'
        )

    lines.append("};")
    lines.append("")
    lines.append(
        "constexpr auto kHvmInstCount = sizeof(kHvmInstTable) / sizeof(kHvmInstTable[0]);"
    )
    return "\n".join(lines)


def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input.csv> <output.hpp>", file=sys.stderr)
        sys.exit(1)

    csv_path = sys.argv[1]
    out_path = sys.argv[2]

    rows = parse_csv(csv_path)

    guard = "HVM_SIM_OPCODE_TABLE_HPP"
    parts = [
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        '#include <cstdint>',
        "",
        make_mnemonic_enum(rows),
        "",
        make_inst_table(rows),
        "",
        f"#endif // {guard}",
    ]

    with open(out_path, "w") as f:
        f.write("\n".join(parts) + "\n")

    print(f"Generated {len(rows)} instruction entries -> {out_path}")


if __name__ == "__main__":
    main()
