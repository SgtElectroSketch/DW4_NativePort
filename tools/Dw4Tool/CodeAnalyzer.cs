internal sealed record CodeSeed(
    int Bank,
    int Address,
    int? EndExclusive,
    string Source,
    string Reason,
    bool FollowTargets = true,
    bool IsEntryPoint = true,
    bool Supplementary = false);

internal sealed record CodeExclusion(int Bank, int Start, int EndExclusive, string Reason);

internal sealed record DecodedInstruction(int Bank, int Address, Opcode Opcode, byte Operand1, byte Operand2)
{
    public int Offset => Address - CodeAnalyzer.CpuBase(Bank);

    public int? Target => Opcode.Mode switch
    {
        AddressingMode.Relative => (Address + 2 + unchecked((sbyte)Operand1)) & 0xFFFF,
        AddressingMode.Absolute when Opcode.IsCall || Opcode.IsJump => Operand1 | (Operand2 << 8),
        _ => null
    };
}

// The facts a following walk held on arrival at an instruction. Branch pruning depends on both, so an
// instruction counts as explored only for arrivals whose facts these cover; null is the widest value.
// Exploration also depends on the walk's window: a bounded walk stops at its end and bounds the targets
// inside it, so facts are kept per window and only an unbounded walk covers every other arrival.
internal readonly record struct PathFacts(bool? ZeroFlag, string? PreviousBranch)
{
    public PathFacts Join(PathFacts other) => new(
        ZeroFlag == other.ZeroFlag ? ZeroFlag : null,
        PreviousBranch == other.PreviousBranch ? PreviousBranch : null);
}

internal sealed class BankAnalysis
{
    public SortedDictionary<int, DecodedInstruction> Instructions { get; } = [];
    public SortedSet<int> InlineDataOffsets { get; } = [];
    public HashSet<int> LabelAddresses { get; } = [];
    public List<string> Warnings { get; } = [];
    public SortedSet<int> ContradictedSupplementarySeeds { get; } = [];
    public SortedSet<int> RejectedSupplementaryBlocks { get; } = [];
    public Dictionary<(int Offset, int WindowStart, int WindowEnd), PathFacts> FollowedFacts { get; } = [];
}

internal static class CodeAnalyzer
{
    private const int BankSize = 0x4000;

    public static int CpuBase(int bank) => IsFixedBank(bank) ? 0xC000 : 0x8000;

    public static Dictionary<int, BankAnalysis> Analyze(
        ReadOnlyMemory<byte> prg,
        IReadOnlyList<CodeSeed> seeds,
        IReadOnlyList<CodeExclusion> exclusions,
        InlineOperandAbi abi,
        IReadOnlyList<CodeExclusion>? declaredData = null)
    {
        Dictionary<int, BankAnalysis> analyses = Enumerable.Range(0, 32)
            .ToDictionary(bank => bank, _ => new BankAnalysis());
        bool[][] occupiedInstructionBytes = Enumerable.Range(0, 32)
            .Select(_ => new bool[BankSize])
            .ToArray();
        bool[][] excludedBytes = Enumerable.Range(0, 32)
            .Select(_ => new bool[BankSize])
            .ToArray();
        foreach (CodeExclusion exclusion in exclusions.Where(exclusion => exclusion.Bank is >= 0 and < 32))
        {
            int cpuBase = CpuBase(exclusion.Bank);
            int start = Math.Max(0, exclusion.Start - cpuBase);
            int endExclusive = Math.Min(BankSize, exclusion.EndExclusive - cpuBase);
            for (int offset = start; offset < endExclusive; offset++)
            {
                excludedBytes[exclusion.Bank][offset] = true;
            }
        }

        Queue<(int Bank, int Address, int? EndExclusive, bool FollowTargets)> pending = new();
        // A following walk explores successors that a non-following or bounded visit of the
        // same address does not, and two windows from one address end in different places, so
        // each visit mode and each window end is queued independently.
        HashSet<(int Bank, int Address, bool FollowTargets, int EndExclusive)> queued = [];
        List<Action>? journal = null;
        int journaledWarnings = 0;
        bool[][] declaredDataBytes = Enumerable.Range(0, 32)
            .Select(_ => new bool[BankSize])
            .ToArray();
        foreach (CodeExclusion range in declaredData ?? [])
        {
            for (int address = range.Start; address < range.EndExclusive; address++)
            {
                declaredDataBytes[range.Bank][address - CpuBase(range.Bank)] = true;
            }
        }
        List<(int Bank, int Offset)>? journaledInstructions = null;

        // Curated, pointer, runtime, and flow evidence decode first.
        foreach (CodeSeed seed in seeds.Where(seed => !seed.Supplementary))
        {
            Enqueue(seed.Bank, seed.Address, seed.EndExclusive, seed.FollowTargets, seed.IsEntryPoint);
        }

        Drain();

        // Supplementary observations (imported Ghidra blocks) only fill bytes that evidence
        // left undecoded. A seed that starts inside an established instruction or inline
        // operand is contradicted and dropped. A block whose decode, including every path it
        // opens, raises any warning is rolled back as a whole: heuristic evidence must be
        // self-consistent before it may classify bytes.
        foreach (IGrouping<(int Bank, int? EndExclusive, string Reason), CodeSeed> block in seeds
            .Where(seed => seed.Supplementary)
            .GroupBy(seed => (seed.Bank, seed.EndExclusive, seed.Reason)))
        {
            List<CodeSeed> contradicted = block.Where(seed =>
            {
                int offset = seed.Address - CpuBase(seed.Bank);
                return AddressMapsToBank(seed.Bank, seed.Address) &&
                    occupiedInstructionBytes[seed.Bank][offset] &&
                    !analyses[seed.Bank].Instructions.ContainsKey(offset);
            }).ToList();
            if (contradicted.Count != 0)
            {
                foreach (CodeSeed seed in contradicted)
                {
                    analyses[seed.Bank].ContradictedSupplementarySeeds.Add(seed.Address);
                }
                analyses[block.Key.Bank].RejectedSupplementaryBlocks.Add(block.Min(seed => seed.Address));
                continue;
            }

            journal = [];
            journaledWarnings = 0;
            journaledInstructions = [];
            foreach (CodeSeed seed in block)
            {
                Enqueue(seed.Bank, seed.Address, seed.EndExclusive, seed.FollowTargets, seed.IsEntryPoint);
            }

            Drain();

            // Supplementary evidence may not reclassify bytes that a verified content range declares.
            bool overlapsDeclaredData = journaledInstructions.Any(item =>
                Enumerable.Range(item.Offset, analyses[item.Bank].Instructions[item.Offset].Opcode.Size)
                    .Any(offset => offset < BankSize && declaredDataBytes[item.Bank][offset]));
            if (journaledWarnings != 0 || overlapsDeclaredData)
            {
                for (int index = journal.Count - 1; index >= 0; index--)
                {
                    journal[index]();
                }

                analyses[block.Key.Bank].RejectedSupplementaryBlocks.Add(block.Min(seed => seed.Address));
            }

            journal = null;
            journaledInstructions = null;
        }

        return analyses;

        void Drain()
        {
            while (pending.TryDequeue(out (int Bank, int Address, int? EndExclusive, bool FollowTargets) entry))
            {
                AnalyzePath(entry.Bank, entry.Address, entry.EndExclusive, entry.FollowTargets);
            }
        }

        void Enqueue(
            int bank,
            int address,
            int? endExclusive = null,
            bool followTargets = true,
            bool isLabelTarget = false)
        {
            if (bank is < 0 or >= 32 || !AddressMapsToBank(bank, address))
            {
                return;
            }

            if (isLabelTarget && analyses[bank].LabelAddresses.Add(address))
            {
                journal?.Add(() => analyses[bank].LabelAddresses.Remove(address));
            }

            (int, int, bool, int) key = (bank, address, followTargets, endExclusive ?? 0);
            if (!queued.Add(key))
            {
                return;
            }

            journal?.Add(() => queued.Remove(key));
            pending.Enqueue((bank, address, endExclusive, followTargets));
        }

        void AddWarning(int bank, string message)
        {
            List<string> warnings = analyses[bank].Warnings;
            warnings.Add(message);
            journaledWarnings++;
            journal?.Add(() => warnings.RemoveAt(warnings.Count - 1));
        }

        void Occupy(int bank, int offset)
        {
            if (!occupiedInstructionBytes[bank][offset])
            {
                occupiedInstructionBytes[bank][offset] = true;
                journal?.Add(() => occupiedInstructionBytes[bank][offset] = false);
            }
        }

        // Inline operands are consumed by the callee's ABI and never fetched as opcodes,
        // so a path entering one is reported exactly like a path entering an operand.
        void ClaimInlineOperands(int bank, int firstOffset, int count)
        {
            BankAnalysis analysis = analyses[bank];
            for (int offset = firstOffset; offset < firstOffset + count && offset < BankSize; offset++)
            {
                if (occupiedInstructionBytes[bank][offset] && !analysis.InlineDataOffsets.Contains(offset))
                {
                    AddWarning(bank, $"${CpuBase(bank) + offset:X4}: control flow enters an existing instruction operand");
                }

                if (analysis.InlineDataOffsets.Add(offset))
                {
                    int claimed = offset;
                    journal?.Add(() => analysis.InlineDataOffsets.Remove(claimed));
                }

                Occupy(bank, offset);
            }
        }

        void AnalyzePath(int bank, int startAddress, int? endExclusive, bool followTargets)
        {
            BankAnalysis analysis = analyses[bank];
            int address = startAddress;
            bool? zeroFlag = null;
            string? previousBranch = null;
            (int Start, int End) window = endExclusive is int windowEnd ? (startAddress, windowEnd) : (0, 0);
            while (AddressMapsToBank(bank, address) && (endExclusive is null || address < endExclusive))
            {
                int offset = address - CpuBase(bank);
                if (excludedBytes[bank][offset])
                {
                    return;
                }

                // An instruction decoded only by a non-following observation (a single executed
                // runtime instruction) has unexplored successors, so a following path walks
                // through it instead of stopping.
                if (analysis.Instructions.TryGetValue(offset, out DecodedInstruction? existing))
                {
                    if (!followTargets)
                    {
                        return;
                    }

                    // An earlier walk that arrived with narrower facts may have pruned a branch
                    // edge this arrival can take, and an earlier bounded walk stopped at its own
                    // window end, so the instruction is walked again unless an unbounded walk or
                    // a walk of this same window already covered these facts. Facts only widen,
                    // so every instruction is revisited a bounded number of times and
                    // reachability does not depend on seed order.
                    PathFacts arrival = new(zeroFlag, previousBranch);
                    if (analysis.FollowedFacts.TryGetValue((offset, 0, 0), out PathFacts unbounded) &&
                        unbounded.Join(arrival) == unbounded)
                    {
                        return;
                    }

                    if (analysis.FollowedFacts.TryGetValue((offset, window.Start, window.End), out PathFacts explored))
                    {
                        PathFacts widened = explored.Join(arrival);
                        if (widened == explored)
                        {
                            return;
                        }

                        zeroFlag = widened.ZeroFlag;
                        previousBranch = widened.PreviousBranch;
                    }
                }

                DecodedInstruction instruction;
                Opcode opcode;
                int prgOffset = (bank * BankSize) + offset;
                if (existing is not null)
                {
                    instruction = existing;
                    opcode = existing.Opcode;
                }
                else
                {
                    if (occupiedInstructionBytes[bank][offset])
                    {
                        AddWarning(bank, $"${address:X4}: control flow enters an existing instruction operand");
                        return;
                    }

                    byte value = prg.Span[prgOffset];
                    Opcode? decoded = OpcodeTable.Get(value);
                    if (decoded is null)
                    {
                        AddWarning(bank, $"${address:X4}: unsupported opcode ${value:X2}; path stopped");
                        return;
                    }

                    opcode = decoded;
                    if (offset + opcode.Size > BankSize)
                    {
                        AddWarning(bank, $"${address:X4}: instruction crosses the physical bank boundary");
                        return;
                    }

                    byte operand1 = opcode.Size >= 2 ? prg.Span[prgOffset + 1] : (byte)0;
                    byte operand2 = opcode.Size == 3 ? prg.Span[prgOffset + 2] : (byte)0;
                    instruction = new(bank, address, opcode, operand1, operand2);
                    if (InvalidControlTransfer(prg.Span, instruction, abi) is string invalid)
                    {
                        AddWarning(bank, $"${address:X4}: {invalid}; path stopped");
                        return;
                    }

                    analysis.Instructions.Add(offset, instruction);
                    journal?.Add(() => analysis.Instructions.Remove(offset));
                    journaledInstructions?.Add((bank, offset));
                    for (int index = 0; index < opcode.Size; index++)
                    {
                        Occupy(bank, offset + index);
                    }
                }

                if (followTargets)
                {
                    (int, int, int) followed = (offset, window.Start, window.End);
                    PathFacts facts = new(zeroFlag, previousBranch);
                    bool revisited = analysis.FollowedFacts.TryGetValue(followed, out PathFacts previousFacts);
                    analysis.FollowedFacts[followed] = facts;
                    journal?.Add(() =>
                    {
                        if (revisited)
                        {
                            analysis.FollowedFacts[followed] = previousFacts;
                        }
                        else
                        {
                            analysis.FollowedFacts.Remove(followed);
                        }
                    });
                }

                // Both fixed banks share the same IRQ/BRK dispatcher, so a fixed-bank BRK
                // consumes its inline operands exactly like a switchable-bank BRK.
                if (opcode.Mnemonic == "brk")
                {
                    int operandCount = InlineOperandCount(prg.Span, instruction, abi);
                    ClaimInlineOperands(bank, offset + 1, operandCount);

                    address += 1 + operandCount;
                    zeroFlag = null;
                    previousBranch = null;

                    // A bounded observation window must not sever the verified inline-operand
                    // service ABI. Imported Ghidra blocks end immediately after a BRK because
                    // Ghidra treats it as terminal, so the operands the service consumes always
                    // fall outside the block. The service returns to the byte after its operands,
                    // so resume there without the window's bound.
                    if (endExclusive is not null && address >= endExclusive)
                    {
                        Enqueue(bank, address, endExclusive: null, followTargets);
                        return;
                    }

                    continue;
                }

                if (instruction.Target is int target)
                {
                    if (!followTargets)
                    {
                        // Runtime observations classify only the instruction that executed.
                    }
                    else if (opcode.IsBranch)
                    {
                        bool? branchTaken = opcode.Mnemonic switch
                        {
                            "beq" when zeroFlag is not null => zeroFlag,
                            "bne" when zeroFlag is not null => !zeroFlag,
                            _ => null
                        };
                        if (branchTaken != false)
                        {
                            int? targetLimit = endExclusive is not null && target >= startAddress && target < endExclusive
                                ? endExclusive
                                : null;
                            Enqueue(bank, target, targetLimit, isLabelTarget: true);
                        }

                        // A branch that immediately follows its complement on the same flag
                        // (for example BCS then BCC) is taken whenever it is reached by fallthrough.
                        if (branchTaken == true || IsComplementaryBranch(previousBranch, opcode.Mnemonic))
                        {
                            return;
                        }
                    }
                    else if (ResolveTargetBank(bank, target) is int targetBank)
                    {
                        int? targetLimit = targetBank == bank && endExclusive is not null &&
                            target >= startAddress && target < endExclusive
                                ? endExclusive
                                : null;
                        Enqueue(targetBank, target, targetLimit, isLabelTarget: true);
                    }
                }

                int inlineCallOperandCount = InlineOperandCount(prg.Span, instruction, abi);
                if (inlineCallOperandCount > 0)
                {
                    ClaimInlineOperands(bank, offset + opcode.Size, inlineCallOperandCount);

                    address += opcode.Size + inlineCallOperandCount;
                    zeroFlag = null;
                    previousBranch = null;
                    continue;
                }

                if (opcode.StopsFlow)
                {
                    return;
                }

                zeroFlag = UpdatedZeroFlag(instruction, zeroFlag, prg.Span);
                previousBranch = opcode.IsBranch ? opcode.Mnemonic : null;
                address += opcode.Size;
            }
        }

    }

    // Bytes following a BRK or JSR that the callee's ABI consumes as inline operands. They are never
    // fetched as opcodes, so every walk over decoded code must step past them.
    public static int InlineOperandCount(ReadOnlySpan<byte> prg, DecodedInstruction instruction, InlineOperandAbi abi)
    {
        if (instruction.Opcode.Mnemonic == "brk")
        {
            return BrkOperandCount(prg, instruction.Bank, instruction.Address, abi);
        }

        return instruction.Opcode.IsCall && instruction.Target is int target &&
            ResolveTargetBank(instruction.Bank, target) is int targetBank
                ? abi.JsrInlineOperandCount(targetBank, target)
                : 0;
    }

    // Where execution continues in the same routine body after this instruction: the next opcode, or
    // the byte after the inline operands once a BRK service or inline-operand call returns. Null when
    // the instruction ends its path (RTS, RTI, JMP).
    public static int? ResumeAddress(ReadOnlySpan<byte> prg, DecodedInstruction instruction, InlineOperandAbi abi) =>
        instruction.Opcode.StopsFlow && instruction.Opcode.Mnemonic != "brk"
            ? null
            : instruction.Address + instruction.Opcode.Size + InlineOperandCount(prg, instruction, abi);

    private static bool IsComplementaryBranch(string? previous, string current) => (previous, current) switch
    {
        ("bcc", "bcs") or ("bcs", "bcc") or ("beq", "bne") or ("bne", "beq") or
        ("bmi", "bpl") or ("bpl", "bmi") or ("bvc", "bvs") or ("bvs", "bvc") => true,
        _ => false
    };

    // Instructions whose execution cannot be valid: a default-family BRK routes through the
    // $8000 service directory of the bank its selector names, so a bank without one would
    // dispatch through unrelated bytes; JSR/JMP into $0800-$5FFF reaches RAM mirrors, PPU/APU
    // registers, or open bus rather than program code.
    private static string? InvalidControlTransfer(ReadOnlySpan<byte> prg, DecodedInstruction instruction, InlineOperandAbi abi)
    {
        if (instruction.Opcode.Mnemonic == "brk")
        {
            int offset = (instruction.Bank * BankSize) + instruction.Offset;
            if (instruction.Offset + 2 >= BankSize)
            {
                return null;
            }

            byte service = prg[offset + 1];
            byte selector = prg[offset + 2];
            int? serviceBank = InlineOperandAbi.DefaultServiceBank(selector);
            return serviceBank is int bank && !abi.ServiceBanks.Contains(bank)
                ? $"BRK ${service:X2},${selector:X2} selects bank ${bank:X2}, which has no service directory"
                : null;
        }

        return (instruction.Opcode.IsCall || instruction.Opcode.IsJump) &&
            instruction.Opcode.Mode == AddressingMode.Absolute &&
            instruction.Target is >= 0x0800 and < 0x6000
                ? $"{instruction.Opcode.Mnemonic} ${instruction.Target:X4} targets hardware or unmapped address space"
                : null;
    }

    // Evidence seeds are facts, not a schedule: walking them in the opposite order must decode the
    // same instructions, inline operands, labels, and warnings. Supplementary blocks keep their order
    // because each is accepted or rolled back against what the blocks before it established.
    public static void ValidateSeedOrderIndependence(
        ReadOnlyMemory<byte> prg,
        IReadOnlyList<CodeSeed> seeds,
        IReadOnlyList<CodeExclusion> exclusions,
        InlineOperandAbi abi,
        IReadOnlyList<CodeExclusion> declaredData,
        IReadOnlyDictionary<int, BankAnalysis> analyses)
    {
        List<CodeSeed> reordered = seeds.Where(seed => !seed.Supplementary).Reverse().ToList();
        reordered.AddRange(seeds.Where(seed => seed.Supplementary));
        Dictionary<int, BankAnalysis> reversed = Analyze(prg, reordered, exclusions, abi, declaredData);
        List<string> errors = [];
        foreach ((int bank, BankAnalysis analysis) in analyses)
        {
            BankAnalysis other = reversed[bank];
            int cpuBase = CpuBase(bank);
            Compare("instruction", analysis.Instructions.Keys, other.Instructions.Keys);
            Compare("inline operand", analysis.InlineDataOffsets, other.InlineDataOffsets);
            Compare("label", analysis.LabelAddresses.Select(address => address - cpuBase),
                other.LabelAddresses.Select(address => address - cpuBase));
            if (!analysis.Warnings.Order(StringComparer.Ordinal).SequenceEqual(other.Warnings.Order(StringComparer.Ordinal)))
            {
                errors.Add($"bank ${bank:X2} warnings differ");
            }

            void Compare(string kind, IEnumerable<int> forward, IEnumerable<int> backward)
            {
                HashSet<int> difference = forward.ToHashSet();
                difference.SymmetricExceptWith(backward);
                if (difference.Count != 0)
                {
                    errors.Add($"bank ${bank:X2} {kind} sets differ at " +
                        string.Join(", ", difference.Order().Take(8).Select(offset => $"${cpuBase + offset:X4}")));
                }
            }
        }

        if (errors.Count != 0)
        {
            throw new InvalidDataException(
                $"analysis depends on evidence seed order ({errors.Count}): {string.Join("; ", errors.Take(20))}");
        }
    }

    public static void ValidateGuardedFlowRecovery(
        ReadOnlyMemory<byte> prg,
        IReadOnlyList<CodeSeed> guardedSeeds,
        IReadOnlyDictionary<int, BankAnalysis> baseline,
        IReadOnlyDictionary<int, BankAnalysis> recovered)
    {
        HashSet<(int Bank, int Address)> baselineDataReferences = [];
        foreach ((int bank, BankAnalysis analysis) in baseline)
        {
            foreach (DecodedInstruction instruction in analysis.Instructions.Values)
            {
                if (instruction.Opcode.Mode is not (
                    AddressingMode.Absolute or AddressingMode.AbsoluteX or AddressingMode.AbsoluteY))
                {
                    continue;
                }

                int address = instruction.Operand1 | (instruction.Operand2 << 8);
                if (ResolveTargetBank(bank, address) == bank)
                {
                    baselineDataReferences.Add((bank, address));
                }
            }
        }

        HashSet<(int Bank, int Address)> baselineStarts = baseline
            .SelectMany(pair => pair.Value.Instructions.Values.Select(instruction =>
                (pair.Key, instruction.Address)))
            .ToHashSet();
        HashSet<(int Bank, int Address)> recoveredBytes = [];
        foreach ((int bank, BankAnalysis analysis) in recovered)
        {
            HashSet<int> baselineOffsets = baseline[bank].Instructions.Values
                .SelectMany(instruction => Enumerable.Range(instruction.Offset, instruction.Opcode.Size))
                .ToHashSet();
            foreach (DecodedInstruction instruction in analysis.Instructions.Values)
            {
                for (int index = 0; index < instruction.Opcode.Size; index++)
                {
                    if (!baselineOffsets.Contains(instruction.Offset + index))
                    {
                        recoveredBytes.Add((bank, instruction.Address + index));
                    }
                }
            }
        }

        List<string> errors = [];
        foreach ((int bank, int address) in baselineDataReferences.Where(recoveredBytes.Contains))
        {
            errors.Add($"recovered instruction bytes cover data reference bank ${bank:X2}:${address:X4}");
        }

        foreach (CodeSeed seed in guardedSeeds)
        {
            string expectedTerminator = seed.Reason.Contains("after RTS", StringComparison.Ordinal) ? "rts" :
                seed.Reason.Contains("after RTI", StringComparison.Ordinal) ? "rti" :
                seed.Reason.Contains("after JMP", StringComparison.Ordinal) ? "jmp" : string.Empty;
            int seedOffset = (seed.Bank * BankSize) + seed.Address - CpuBase(seed.Bank);
            bool validPredecessor = expectedTerminator switch
            {
                "rts" => seedOffset >= 1 && prg.Span[seedOffset - 1] == 0x60,
                "rti" => seedOffset >= 1 && prg.Span[seedOffset - 1] == 0x40,
                "jmp" => seedOffset >= 3 && prg.Span[seedOffset - 3] == 0x4C,
                _ => false
            };
            if (!validPredecessor)
            {
                errors.Add($"guarded seed bank ${seed.Bank:X2}:${seed.Address:X4} lacks its declared predecessor terminator");
                continue;
            }

            BankAnalysis recoveredBank = recovered[seed.Bank];
            if (!recoveredBank.Instructions.TryGetValue(seed.Address - CpuBase(seed.Bank), out _))
            {
                errors.Add($"guarded seed bank ${seed.Bank:X2}:${seed.Address:X4} was not decoded");
                continue;
            }

            if (!PathReachesTerminatorOrMerge(seed, baselineStarts, recovered))
            {
                errors.Add($"guarded seed bank ${seed.Bank:X2}:${seed.Address:X4} does not terminate or merge cleanly");
            }
        }

        if (errors.Count != 0)
        {
            throw new InvalidDataException(
                $"guarded flow recovery validation failed ({errors.Count}): {string.Join("; ", errors.Take(20))}");
        }

        static bool PathReachesTerminatorOrMerge(
            CodeSeed seed,
            IReadOnlySet<(int Bank, int Address)> baselineStarts,
            IReadOnlyDictionary<int, BankAnalysis> recovered)
        {
            Queue<int> pending = new();
            HashSet<int> visited = [];
            pending.Enqueue(seed.Address);
            while (pending.TryDequeue(out int address))
            {
                if (baselineStarts.Contains((seed.Bank, address)))
                {
                    return true;
                }
                if (!visited.Add(address) ||
                    !recovered[seed.Bank].Instructions.TryGetValue(address - CpuBase(seed.Bank), out DecodedInstruction? instruction))
                {
                    continue;
                }
                if (instruction.Opcode.Mnemonic is "rts" or "rti" || instruction.Opcode.IsJump)
                {
                    return true;
                }

                List<int> successors = [];
                int sequential = instruction.Address + instruction.Opcode.Size;
                while (recovered[seed.Bank].InlineDataOffsets.Contains(sequential - CpuBase(seed.Bank)))
                {
                    sequential++;
                }
                if (instruction.Opcode.IsBranch && instruction.Target is int branchTarget &&
                    (baselineStarts.Contains((seed.Bank, branchTarget)) ||
                     recovered[seed.Bank].Instructions.ContainsKey(branchTarget - CpuBase(seed.Bank))))
                {
                    successors.Add(branchTarget);
                }
                if (baselineStarts.Contains((seed.Bank, sequential)) ||
                    recovered[seed.Bank].Instructions.ContainsKey(sequential - CpuBase(seed.Bank)))
                {
                    successors.Add(sequential);
                }
                if (successors.Count == 0)
                {
                    continue;
                }
                foreach (int successor in successors)
                {
                    pending.Enqueue(successor);
                }
            }
            return false;
        }
    }

    private static int BrkOperandCount(
        ReadOnlySpan<byte> prg,
        int bank,
        int address,
        InlineOperandAbi abi)
    {
        int offset = address - CpuBase(bank);
        if (offset + 2 >= BankSize)
        {
            return Math.Max(0, BankSize - offset - 1);
        }

        int prgOffset = (bank * BankSize) + offset;
        int operandCount = abi.BrkOperandCount(prg[prgOffset + 1], prg[prgOffset + 2]);
        return Math.Min(operandCount, BankSize - offset - 1);
    }

    private static bool AddressMapsToBank(int bank, int address)
    {
        int cpuBase = CpuBase(bank);
        return address >= cpuBase && address < cpuBase + BankSize;
    }

    public static int? ResolveTargetBank(int currentBank, int target)
    {
        if (target is >= 0xC000 and <= 0xFFFF)
        {
            return currentBank < 0x10 ? 0x0F : 0x1F;
        }

        if (target is >= 0x8000 and < 0xC000 && !IsFixedBank(currentBank))
        {
            return currentBank;
        }

        return null;
    }

    private static bool IsFixedBank(int bank) => (bank & 0x0F) == 0x0F;

    private static bool? UpdatedZeroFlag(
        DecodedInstruction instruction,
        bool? previous,
        ReadOnlySpan<byte> prg)
    {
        if (instruction.Opcode.Mode == AddressingMode.Immediate &&
            instruction.Opcode.Mnemonic is "lda" or "ldx" or "ldy")
        {
            return instruction.Operand1 == 0;
        }

        if (instruction.Opcode.Mode == AddressingMode.Absolute &&
            instruction.Opcode.Mnemonic is "lda" or "ldx" or "ldy")
        {
            int address = instruction.Operand1 | (instruction.Operand2 << 8);
            if (ResolveTargetBank(instruction.Bank, address) is int bank)
            {
                int offset = (bank * BankSize) + address - CpuBase(bank);
                return prg[offset] == 0;
            }
        }

        return instruction.Opcode.Mnemonic switch
        {
            "bcc" or "bcs" or "beq" or "bmi" or "bne" or "bpl" or "bvc" or "bvs" or
            "clc" or "cld" or "cli" or "clv" or "jmp" or "nop" or "pha" or "php" or
            "sec" or "sed" or "sei" or "sta" or "stx" or "sty" or "txs" => previous,
            _ => null
        };
    }
}