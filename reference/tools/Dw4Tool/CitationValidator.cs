using System.Globalization;
using System.Text.RegularExpressions;

// Evidence text cites instructions as "MNEMONIC operand at $ADDR" (or a slash-separated group
// "A / B at $ADDR-$END"). Every cited instruction must be decoded code at the cited place, so a reason
// cannot silently drift from the analysis it claims to rest on.
//
// The check is closed by one invariant rather than by guessing what malformed text looks like: every
// 6502 mnemonic in validated text must lie inside a citation this class parsed and checked. A mnemonic
// with a mistyped operand, a missing or overlong address, a broken group, lowercase spelling, or no
// place at all is therefore an error, never an unchecked claim. Prose that describes code without
// naming an instruction is not a citation and is not checked.
internal static class CitationValidator
{
    private const string Mnemonics =
        "ADC|AND|ASL|BCC|BCS|BEQ|BIT|BMI|BNE|BPL|BRK|BVC|BVS|CLC|CLD|CLI|CLV|CMP|CPX|CPY|DEC|DEX|DEY|EOR|INC|INX|" +
        "INY|JMP|JSR|LDA|LDX|LDY|LSR|NOP|ORA|PHA|PHP|PLA|PLP|ROL|ROR|RTI|RTS|SBC|SEC|SED|SEI|STA|STX|STY|TAX|TAY|" +
        "TSX|TXA|TXS|TYA";

    // A hexadecimal field must end where the pattern ends, so "$8FD5F" is not read as "$8FD5".
    private const string HexEnd = "(?![0-9A-Fa-f])";

    // BRK operand bytes, then immediate, zero-page, and absolute operands with an optional index, then
    // ($ZP,X), ($ZP),Y, and ($ADDR).
    private const string Operand =
        @"(?:\$[0-9A-F]{2}(?:,\$[0-9A-F]{2}){1,3}" + HexEnd +
        @"|#?\$[0-9A-F]{2,4}" + HexEnd + @"(?:,[XY])?" +
        @"|\(\$[0-9A-F]{2},X\)|\(\$[0-9A-F]{2}\),Y|\(\$[0-9A-F]{4}\))";

    private const string Element = @"(?:BRK(?:\s+service)?|" + Mnemonics + @")(?:\s+" + Operand + ")?";

    private const string Place =
        @"\s+at\s+(?:bank \$(?<bank>[0-9A-F]{2}):)?\$(?<start>[0-9A-F]{4})" + HexEnd +
        @"(?:-\$(?<end>[0-9A-F]{4})" + HexEnd + ")?";

    private static readonly Regex Citation = new(
        @"(?<![A-Za-z0-9$#])((?:" + Element + @"\s*/\s*)*" + Element + ")" + Place,
        RegexOptions.Compiled);

    private static readonly Regex MnemonicToken = new(
        @"(?<![A-Za-z0-9$#])(?:" + Mnemonics + ")(?![A-Za-z0-9])",
        RegexOptions.Compiled | RegexOptions.IgnoreCase);

    // The only mnemonics that are also ordinary lowercase words in this evidence text.
    private static readonly HashSet<string> EnglishWords = new(StringComparer.Ordinal) { "and", "bit" };

    public static void Validate(
        IEnumerable<(int Bank, string Owner, string Reason)> reasons,
        ReadOnlyMemory<byte> prg,
        IReadOnlyDictionary<int, BankAnalysis> analyses)
    {
        List<string> errors = [];
        foreach ((int ownerBank, string owner, string reason) in reasons)
        {
            MatchCollection citations = Citation.Matches(reason);
            foreach (Match token in MnemonicToken.Matches(reason))
            {
                bool english = token.Value != token.Value.ToUpperInvariant() &&
                    EnglishWords.Contains(token.Value.ToLowerInvariant());
                if (!english && !citations.Any(citation =>
                    citation.Index <= token.Index && token.Index + token.Length <= citation.Index + citation.Length))
                {
                    int contextStart = Math.Max(0, token.Index - 24);
                    int contextEnd = Math.Min(reason.Length, token.Index + token.Length + 24);
                    errors.Add($"{owner} names '{token.Value}' outside a citation that can be checked: " +
                        $"...{reason[contextStart..contextEnd]}...");
                }
            }

            foreach (Match match in citations)
            {
                int bank = match.Groups["bank"].Success
                    ? int.Parse(match.Groups["bank"].Value, NumberStyles.HexNumber, CultureInfo.InvariantCulture)
                    : ownerBank;
                int start = int.Parse(match.Groups["start"].Value, NumberStyles.HexNumber, CultureInfo.InvariantCulture);
                int end = match.Groups["end"].Success
                    ? int.Parse(match.Groups["end"].Value, NumberStyles.HexNumber, CultureInfo.InvariantCulture)
                    : start;
                if (start >= 0xC000 && (bank & 0x0F) != 0x0F)
                {
                    bank = bank < 0x10 ? 0x0F : 0x1F;
                }

                BankAnalysis analysis = analyses[bank];
                List<string> window = analysis.Instructions.Values
                    .Where(instruction => instruction.Address >= start && instruction.Address <= end)
                    .Select(instruction => Format(instruction, analysis, prg.Span))
                    .ToList();
                foreach (string part in Regex.Split(match.Groups[1].Value.Trim(), @"\s*/\s*"))
                {
                    string cited = Regex.Replace(part.Trim(), @"\s+", " ").Replace("BRK service", "BRK", StringComparison.Ordinal);
                    bool mnemonicOnly = !cited.Contains(' ');
                    bool brkPrefix = cited.StartsWith("BRK ", StringComparison.Ordinal);
                    if (!window.Any(text => text.Equals(cited, StringComparison.Ordinal) ||
                        mnemonicOnly && text.Split(' ')[0].Equals(cited, StringComparison.Ordinal) ||
                        brkPrefix && text.StartsWith(cited + ",", StringComparison.Ordinal)))
                    {
                        errors.Add($"{owner} cites '{cited}' at bank ${bank:X2}:${start:X4}" +
                            (end != start ? $"-${end:X4}" : string.Empty) + ", which is not decoded there");
                    }
                }
            }
        }

        if (errors.Count != 0)
        {
            throw new InvalidDataException($"evidence citation validation failed ({errors.Count}): {string.Join("; ", errors.Take(20))}");
        }
    }

    private static string Format(DecodedInstruction instruction, BankAnalysis analysis, ReadOnlySpan<byte> prg)
    {
        string mnemonic = instruction.Opcode.Mnemonic.ToUpperInvariant();
        if (mnemonic == "BRK")
        {
            // A BRK is cited with the operand bytes its service consumes.
            List<string> operands = [];
            for (int offset = instruction.Offset + 1; analysis.InlineDataOffsets.Contains(offset); offset++)
            {
                operands.Add($"${prg[(instruction.Bank * 0x4000) + offset]:X2}");
            }

            return operands.Count == 0 ? mnemonic : $"{mnemonic} {string.Join(',', operands)}";
        }

        int word = instruction.Operand1 | (instruction.Operand2 << 8);
        string operand = instruction.Opcode.Mode switch
        {
            AddressingMode.Immediate => $"#${instruction.Operand1:X2}",
            AddressingMode.ZeroPage => $"${instruction.Operand1:X2}",
            AddressingMode.ZeroPageX => $"${instruction.Operand1:X2},X",
            AddressingMode.ZeroPageY => $"${instruction.Operand1:X2},Y",
            AddressingMode.Absolute => $"${word:X4}",
            AddressingMode.AbsoluteX => $"${word:X4},X",
            AddressingMode.AbsoluteY => $"${word:X4},Y",
            AddressingMode.Relative => $"${instruction.Target:X4}",
            AddressingMode.Indirect => $"(${word:X4})",
            AddressingMode.IndexedIndirect => $"(${instruction.Operand1:X2},X)",
            AddressingMode.IndirectIndexed => $"(${instruction.Operand1:X2}),Y",
            _ => string.Empty
        };
        return operand.Length == 0 ? mnemonic : $"{mnemonic} {operand}";
    }
}
