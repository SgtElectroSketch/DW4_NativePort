// Traces the caller's status flags (C, N, V, Z) through a routine body and reports two sets.
//
// Reads: flags that some decoded path consumes before any decoded instruction writes them. Direct JSR
// and JMP targets are followed through per-entry summaries, PHP/PLP are matched on a modeled stack so
// saving and restoring flags is not a read, and a branch on a flag the path itself set to a constant
// (SEC, CLC, CLV, an immediate load) follows only its feasible edge. No other feasibility is proven,
// so a listed flag may belong to a path the program never takes.
//
// Unresolved: flags still holding the caller's value where a path left what this trace can follow: a
// BRK service (the dispatcher at $C408 reloads A from $0507 at $C38F before saving flags for the
// service, so only the caller's carry and overflow reach it, and every flag after it returns is the
// service's), an indirect or unresolvable jump or call, undecoded code, a return that does not go back
// to the call site, or an access to the stack page or stack pointer while saved flags are on the
// modeled stack. Whether those flags are read is unknown.
//
// A flag in neither set is written on every decoded path before any decoded read. Neither set is a
// bound on the true dependencies in general: Reads can include infeasible paths, and anything behind
// an Unresolved flag is not examined.
internal sealed class EntryFlagAnalyzer(
    ReadOnlyMemory<byte> prg,
    InlineOperandAbi abi,
    IReadOnlyDictionary<int, BankAnalysis> analyses)
{
    private const int Negative = 1;
    private const int Overflow = 2;
    private const int Zero = 4;
    private const int Carry = 8;
    private const int All = Negative | Overflow | Zero | Carry;

    // The modeled stack holds one base-18 digit per pushed byte: 1-16 for a PHP (its entry-valued
    // flag mask plus one) and 17 for any other byte.
    private const ulong StackRadix = 18;
    private const ulong OtherByte = 17;
    private const int StackDepth = 12;

    private readonly record struct Summary(int Reads, int Passes, int Unresolved, bool Returns);

    private readonly record struct State(
        int Bank,
        int Address,
        int Unwritten,
        int Known,
        int Values,
        ulong Stack,
        int Depth,
        bool Underflow);

    private readonly Dictionary<(int Bank, int Address), Summary> summaries = [];

    public (IReadOnlySet<string> Reads, IReadOnlySet<string> Unresolved) EntryFlags(int bank, int address)
    {
        Summary summary = Summarize((bank, address));
        return (Names(summary.Reads), Names(summary.Unresolved & ~summary.Reads));
    }

    private static SortedSet<string> Names(int mask)
    {
        SortedSet<string> names = new(StringComparer.Ordinal);
        foreach ((int flag, string name) in new[] { (Carry, "C"), (Negative, "N"), (Overflow, "V"), (Zero, "Z") })
        {
            if ((mask & flag) != 0)
            {
                names.Add(name);
            }
        }

        return names;
    }

    // Summaries only grow, and one already at its fixed point depends solely on entries that were
    // settled with it, so each call iterates just the entries it discovers.
    private Summary Summarize((int Bank, int Address) entry)
    {
        if (summaries.TryGetValue(entry, out Summary settled))
        {
            return settled;
        }

        List<(int Bank, int Address)> unsettled = [entry];
        summaries.Add(entry, default);
        bool changed = true;
        while (changed)
        {
            changed = false;
            for (int index = 0; index < unsettled.Count; index++)
            {
                Summary updated = Walk(unsettled[index], unsettled);
                if (updated != summaries[unsettled[index]])
                {
                    summaries[unsettled[index]] = updated;
                    changed = true;
                }
            }
        }

        return summaries[entry];
    }

    private Summary Walk((int Bank, int Address) entry, List<(int Bank, int Address)> unsettled)
    {
        int reads = 0;
        int passes = 0;
        int unresolved = 0;
        bool returns = false;
        HashSet<State> visited = [];
        Stack<State> pending = new();
        pending.Push(new State(entry.Bank, entry.Address, All, 0, 0, 0, 0, false));
        while (pending.TryPop(out State state))
        {
            (int bank, int address, int unwritten, int known, int values, ulong stack, int depth, bool underflow) = state;
            while (visited.Add(new State(bank, address, unwritten, known, values, stack, depth, underflow)))
            {
                // Flags that still hold the caller's value, live or saved on the modeled stack.
                int entryValued = unwritten | SavedFlags(stack);
                if (!analyses[bank].Instructions.TryGetValue(
                    address - CodeAnalyzer.CpuBase(bank),
                    out DecodedInstruction? instruction))
                {
                    unresolved |= entryValued;
                    break;
                }

                Opcode opcode = instruction.Opcode;
                string mnemonic = opcode.Mnemonic;
                bool touchesStackPage = mnemonic is "tsx" or "txs" ||
                    opcode.Mode is AddressingMode.Absolute or AddressingMode.AbsoluteX or AddressingMode.AbsoluteY &&
                    (instruction.Operand1 | (instruction.Operand2 << 8)) is >= 0x0100 and <= 0x01FF;
                if (mnemonic == "txs" || touchesStackPage && SavedFlags(stack) != 0 ||
                    mnemonic is "php" or "pha" && depth == StackDepth)
                {
                    unresolved |= entryValued;
                    break;
                }

                switch (mnemonic)
                {
                    case "php":
                        stack = (stack * StackRadix) + (ulong)unwritten + 1;
                        depth++;
                        break;
                    case "pha":
                        stack = (stack * StackRadix) + OtherByte;
                        depth++;
                        break;
                    case "plp":
                    case "pla":
                        ulong pulled = stack % StackRadix;
                        if (depth == 0)
                        {
                            // The byte belongs to the caller's frame: a return address or data.
                            underflow = true;
                        }
                        else
                        {
                            stack /= StackRadix;
                            depth--;
                        }

                        int saved = pulled is >= 1 and <= 16 ? (int)pulled - 1 : 0;
                        if (mnemonic == "plp")
                        {
                            unwritten = saved;
                            known = 0;
                        }
                        else
                        {
                            // Pulling a saved status byte into A makes A depend on those flags.
                            reads |= saved;
                            unwritten &= ~(Negative | Zero);
                            known &= ~(Negative | Zero);
                        }

                        break;
                    default:
                        (int instructionReads, int instructionWrites) = FlagEffect(mnemonic);
                        reads |= unwritten & instructionReads;
                        unwritten &= ~instructionWrites;
                        known &= ~instructionWrites;
                        (int constantFlags, int constantValues) = ConstantFlags(instruction);
                        known |= constantFlags;
                        values = (values & ~constantFlags) | constantValues;
                        break;
                }

                if (mnemonic == "rts")
                {
                    if (underflow || depth != 0)
                    {
                        // Control leaves through the caller's frame or a pushed address.
                        unresolved |= unwritten | SavedFlags(stack);
                    }
                    else
                    {
                        passes |= unwritten;
                        returns = true;
                    }

                    break;
                }

                if (mnemonic == "brk")
                {
                    unresolved |= unwritten & (Carry | Overflow);
                    unwritten = 0;
                    known = 0;
                }

                if (opcode.IsBranch && instruction.Target is int branchTarget)
                {
                    (int flag, bool takenWhenSet) = BranchCondition(mnemonic);
                    bool? taken = (known & flag) != 0 ? ((values & flag) != 0) == takenWhenSet : null;
                    if (taken != false)
                    {
                        pending.Push(new State(bank, branchTarget, unwritten, known, values, stack, depth, underflow));
                    }

                    if (taken == true)
                    {
                        break;
                    }
                }

                if (opcode.IsJump)
                {
                    if (instruction.Target is not int jumpTarget ||
                        CodeAnalyzer.ResolveTargetBank(bank, jumpTarget) is not int jumpBank)
                    {
                        unresolved |= unwritten | SavedFlags(stack);
                        break;
                    }

                    bank = jumpBank;
                    address = jumpTarget;
                    continue;
                }

                if (opcode.IsCall)
                {
                    if (instruction.Target is not int callTarget ||
                        CodeAnalyzer.ResolveTargetBank(bank, callTarget) is not int callBank ||
                        !analyses[callBank].Instructions.ContainsKey(callTarget - CodeAnalyzer.CpuBase(callBank)))
                    {
                        unresolved |= unwritten | SavedFlags(stack);
                        break;
                    }

                    if (!summaries.TryGetValue((callBank, callTarget), out Summary callee))
                    {
                        summaries.Add((callBank, callTarget), callee);
                        unsettled.Add((callBank, callTarget));
                    }

                    reads |= unwritten & callee.Reads;
                    unresolved |= unwritten & callee.Unresolved;
                    if (!callee.Returns)
                    {
                        break;
                    }

                    unwritten &= callee.Passes;
                    known = 0;
                }

                if (CodeAnalyzer.ResumeAddress(prg.Span, instruction, abi) is not int resume)
                {
                    break;
                }

                address = resume;
            }
        }

        return new Summary(reads, passes, unresolved, returns);
    }

    private static int SavedFlags(ulong stack)
    {
        int saved = 0;
        for (; stack != 0; stack /= StackRadix)
        {
            if (stack % StackRadix is >= 1 and <= 16 and ulong entry)
            {
                saved |= (int)entry - 1;
            }
        }

        return saved;
    }

    private static (int Flag, bool TakenWhenSet) BranchCondition(string mnemonic) => mnemonic switch
    {
        "bcs" => (Carry, true),
        "bcc" => (Carry, false),
        "beq" => (Zero, true),
        "bne" => (Zero, false),
        "bmi" => (Negative, true),
        "bpl" => (Negative, false),
        "bvs" => (Overflow, true),
        _ => (Overflow, false)
    };

    // Flags an instruction leaves at a value fixed by the instruction alone.
    private static (int Flags, int Values) ConstantFlags(DecodedInstruction instruction) =>
        instruction.Opcode.Mnemonic switch
        {
            "sec" => (Carry, Carry),
            "clc" => (Carry, 0),
            "clv" => (Overflow, 0),
            "lda" or "ldx" or "ldy" when instruction.Opcode.Mode == AddressingMode.Immediate =>
                (Negative | Zero,
                    (instruction.Operand1 == 0 ? Zero : 0) | (instruction.Operand1 >= 0x80 ? Negative : 0)),
            _ => (0, 0)
        };

    private static (int Reads, int Writes) FlagEffect(string mnemonic) => mnemonic switch
    {
        "adc" or "sbc" => (Carry, All),
        "rol" or "ror" => (Carry, Negative | Zero | Carry),
        "asl" or "lsr" or "cmp" or "cpx" or "cpy" => (0, Negative | Zero | Carry),
        "and" or "ora" or "eor" or "lda" or "ldx" or "ldy" or "tax" or "tay" or "txa" or "tya" or "tsx" or
            "inc" or "dec" or "inx" or "iny" or "dex" or "dey" => (0, Negative | Zero),
        "bit" => (0, Negative | Overflow | Zero),
        "clc" or "sec" => (0, Carry),
        "clv" => (0, Overflow),
        "bcc" or "bcs" => (Carry, 0),
        "beq" or "bne" => (Zero, 0),
        "bmi" or "bpl" => (Negative, 0),
        "bvc" or "bvs" => (Overflow, 0),
        "rti" => (0, All),
        _ => (0, 0)
    };
}
