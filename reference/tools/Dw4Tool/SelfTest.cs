// Synthetic regression probes for analyzer, interface, and citation logic. Each probe assembles a few
// bytes into an otherwise empty PRG image and runs the production methods against it, so a defect in
// traversal or validation fails the completion gate even when the ROM-derived counts still match.
internal static class SelfTest
{
    private const int BankSize = 0x4000;

    public static int Run(string[] args)
    {
        if (args.Length != 2)
        {
            throw new ArgumentException("usage: self-test <project-root>");
        }

        InlineOperandAbi abi = InlineOperandAbi.Load(
            Path.Combine(Path.GetFullPath(args[1]), "config", "inline-operand-abi.tsv"));
        abi.ServiceBanks = new HashSet<int> { 0x0E };

        (string Name, Func<InlineOperandAbi, string?> Probe)[] probes =
        [
            ("routine interface continues after a returning BRK service", InterfaceContinuesAfterBrk),
            ("routine interface skips inline JSR operands", InterfaceSkipsInlineJsrOperands),
            ("analyzer reachability is independent of seed order under zero-flag pruning", ZeroFlagPruningIsSeedOrderIndependent),
            ("analyzer reachability is independent of seed order under complementary-branch pruning", ComplementaryBranchPruningIsSeedOrderIndependent),
            ("routine interface reports an entry flag read before any write", InterfaceReportsEntryFlagRead),
            ("routine interface does not report a flag written before it is read", InterfaceIgnoresFlagWrittenBeforeRead),
            ("routine interface follows a direct call when tracing entry flags", InterfaceTracesEntryFlagsThroughDirectCall),
            ("routine interface does not count a flag saved by PHP and restored by PLP as read", InterfaceIgnoresSavedAndRestoredFlags),
            ("routine interface reports a flag read after PLP restores its entry value", InterfaceReportsFlagReadAfterRestore),
            ("entry-flag trace follows only the feasible edge of a branch on a flag the path set", EntryFlagsSkipInfeasibleBranch),
            ("entry-flag trace stops after a call that never returns to its call site", EntryFlagsStopAfterNonReturningCall),
            ("entry-flag trace does not count a restored flag as read when the stack page was written", EntryFlagsDistrustAliasedStack),
            ("entry-flag trace leaves carry and overflow unresolved at a BRK service", EntryFlagsUnresolvedAtBrk),
            ("entry-flag trace resumes after a BRK service and sees a restored flag", EntryFlagsResumeAfterBrk),
            ("routine interface follows a tail jump into the fixed bank", InterfaceFollowsCrossBankTailJump),
            ("routine interface lists BRK services and jumps it cannot follow", InterfaceListsServicesAndUnfollowedJumps),
            ("analyzer decodes the same code for two windows from one address in either order", WindowsFromOneAddressAreOrderIndependent),
            ("analyzer decodes the same code for a window and an unbounded entry in either order", WindowAndEntryAreOrderIndependent),
            ("citation validator rejects an indirect citation that is not decoded", CitationRejectsUndecodedIndirect),
            ("citation validator accepts a decoded indirect citation", CitationAcceptsDecodedIndirect),
            ("citation validator rejects instruction-citation text it cannot parse", CitationRejectsUnparseableText),
            ("citation validator rejects every mnemonic that is not part of a parsed citation", CitationRejectsUncitedMnemonics),
            ("citation validator checks BRK citations against the service operand bytes", CitationChecksBrkOperands),
        ];

        int failures = 0;
        foreach ((string name, Func<InlineOperandAbi, string?> probe) in probes)
        {
            string? failure = probe(abi);
            Console.WriteLine(failure is null ? $"PASS {name}" : $"FAIL {name}: {failure}");
            failures += failure is null ? 0 : 1;
        }

        Console.WriteLine($"self-test: {probes.Length - failures}/{probes.Length} probes passed");
        return failures == 0 ? 0 : 1;
    }

    // LDX #$DC; BRK $82,$E7; JSR $8010; STA $DC; RTS, with RTS at $8010. Service $82 takes the pointer
    // path and resumes at BRK+3, so the call and the write belong to the routine body.
    private static string? InterfaceContinuesAfterBrk(InlineOperandAbi abi)
    {
        byte[] prg = new byte[32 * BankSize];
        Place(prg, 0x08, 0x8000, 0xA2, 0xDC, 0x00, 0x82, 0xE7, 0x20, 0x10, 0x80, 0x85, 0xDC, 0x60);
        Place(prg, 0x08, 0x8010, 0x60);
        Dictionary<int, BankAnalysis> analyses = Analyze(prg, abi, (0x08, 0x8000));
        RoutineInterface result = Program.AnalyzeRoutineInterface(0x08, 0x8000, prg, abi, analyses);
        return !result.Calls.Contains("$8010") ? $"calls are [{string.Join(',', result.Calls)}], expected $8010"
            : !result.DirectMemoryWrites.Contains("$DC") ? $"writes are [{string.Join(',', result.DirectMemoryWrites)}], expected $DC"
            : null;
    }

    // JSR $8C18; <one inline byte>; STA $40; RTS. Bank $10:$8C18 is a declared JsrInline callee.
    private static string? InterfaceSkipsInlineJsrOperands(InlineOperandAbi abi)
    {
        byte[] prg = new byte[32 * BankSize];
        Place(prg, 0x10, 0x8000, 0x20, 0x18, 0x8C, 0x05, 0x85, 0x40, 0x60);
        Place(prg, 0x10, 0x8C18, 0x60);
        Dictionary<int, BankAnalysis> analyses = Analyze(prg, abi, (0x10, 0x8000));
        RoutineInterface result = Program.AnalyzeRoutineInterface(0x10, 0x8000, prg, abi, analyses);
        return result.DirectMemoryWrites.Contains("$40")
            ? null
            : $"writes are [{string.Join(',', result.DirectMemoryWrites)}], expected $40";
    }

    // LDA #$00; BNE $8007; RTS; NOP; NOP; RTS. Entered at $8000 the branch is never taken, but an
    // entry at $8002 arrives with an unknown zero flag and must still reach $8007.
    private static string? ZeroFlagPruningIsSeedOrderIndependent(InlineOperandAbi abi)
    {
        byte[] prg = new byte[32 * BankSize];
        Place(prg, 0x10, 0x8000, 0xA9, 0x00, 0xD0, 0x03, 0x60, 0xEA, 0xEA, 0x60);
        Dictionary<int, BankAnalysis> both = Analyze(prg, abi, (0x10, 0x8000), (0x10, 0x8002));
        Dictionary<int, BankAnalysis> reversed = Analyze(prg, abi, (0x10, 0x8002), (0x10, 0x8000));
        return !both[0x10].Instructions.ContainsKey(7) ? "$8007 is undecoded for seeds $8000,$8002"
            : !reversed[0x10].Instructions.ContainsKey(7) ? "$8007 is undecoded for seeds $8002,$8000"
            : both[0x10].Warnings.Count + reversed[0x10].Warnings.Count != 0 ? "probe raised analyzer warnings"
            : null;
    }

    // BCS $8005; BCC $8005; RTS; RTS. Falling through from BCS the BCC is always taken, but an entry
    // at $8002 arrives with an unknown carry and must still reach the fallthrough at $8004.
    private static string? ComplementaryBranchPruningIsSeedOrderIndependent(InlineOperandAbi abi)
    {
        byte[] prg = new byte[32 * BankSize];
        Place(prg, 0x10, 0x8000, 0xB0, 0x03, 0x90, 0x01, 0x60, 0x60);
        Dictionary<int, BankAnalysis> both = Analyze(prg, abi, (0x10, 0x8000), (0x10, 0x8002));
        Dictionary<int, BankAnalysis> reversed = Analyze(prg, abi, (0x10, 0x8002), (0x10, 0x8000));
        return !both[0x10].Instructions.ContainsKey(4) ? "$8004 is undecoded for seeds $8000,$8002"
            : !reversed[0x10].Instructions.ContainsKey(4) ? "$8004 is undecoded for seeds $8002,$8000"
            : null;
    }

    // BEQ $8004; SEC; RTS; CLC; RTS: the returned carry depends only on the caller's zero flag.
    private static string? InterfaceReportsEntryFlagRead(InlineOperandAbi abi)
    {
        byte[] prg = new byte[32 * BankSize];
        Place(prg, 0x10, 0x8000, 0xF0, 0x02, 0x38, 0x60, 0x18, 0x60);
        Dictionary<int, BankAnalysis> analyses = Analyze(prg, abi, (0x10, 0x8000));
        RoutineInterface result = Program.AnalyzeRoutineInterface(0x10, 0x8000, prg, abi, analyses);
        return result.EntryFlagReads.SetEquals(["Z"])
            ? null
            : $"entry flag reads are [{string.Join(',', result.EntryFlagReads)}], expected Z";
    }

    // LDA $40; BEQ $8006; SEC; RTS; CLC; RTS: LDA writes the zero flag before BEQ reads it.
    private static string? InterfaceIgnoresFlagWrittenBeforeRead(InlineOperandAbi abi)
    {
        byte[] prg = new byte[32 * BankSize];
        Place(prg, 0x10, 0x8000, 0xA5, 0x40, 0xF0, 0x02, 0x38, 0x60, 0x18, 0x60);
        Dictionary<int, BankAnalysis> analyses = Analyze(prg, abi, (0x10, 0x8000));
        RoutineInterface result = Program.AnalyzeRoutineInterface(0x10, 0x8000, prg, abi, analyses);
        return result.EntryFlagReads.Count == 0
            ? null
            : $"entry flag reads are [{string.Join(',', result.EntryFlagReads)}], expected none";
    }

    // STA $40; JSR $8010; RTS, with BCC $8013; RTS; RTS at $8010. STA writes no flag, so the callee's
    // carry test still reads the outer caller's carry.
    private static string? InterfaceTracesEntryFlagsThroughDirectCall(InlineOperandAbi abi)
    {
        byte[] prg = new byte[32 * BankSize];
        Place(prg, 0x10, 0x8000, 0x85, 0x40, 0x20, 0x10, 0x80, 0x60);
        Place(prg, 0x10, 0x8010, 0x90, 0x01, 0x60, 0x60);
        Dictionary<int, BankAnalysis> analyses = Analyze(prg, abi, (0x10, 0x8000));
        RoutineInterface result = Program.AnalyzeRoutineInterface(0x10, 0x8000, prg, abi, analyses);
        return result.EntryFlagReads.SetEquals(["C"])
            ? null
            : $"entry flag reads are [{string.Join(',', result.EntryFlagReads)}], expected C";
    }

    // PHP; LDA $40; PLP; RTS preserves the caller's flags without depending on any of them.
    private static string? InterfaceIgnoresSavedAndRestoredFlags(InlineOperandAbi abi)
    {
        byte[] prg = new byte[32 * BankSize];
        Place(prg, 0x10, 0x8000, 0x08, 0xA5, 0x40, 0x28, 0x60);
        Dictionary<int, BankAnalysis> analyses = Analyze(prg, abi, (0x10, 0x8000));
        RoutineInterface result = Program.AnalyzeRoutineInterface(0x10, 0x8000, prg, abi, analyses);
        return result.EntryFlagReads.Count == 0
            ? null
            : $"entry flag reads are [{string.Join(',', result.EntryFlagReads)}], expected none";
    }

    // PHP; LDA $40; PLP; BCC $8007; RTS; RTS: LDA rewrites N and Z, PLP restores the caller's flags,
    // and the carry test then reads the caller's carry.
    private static string? InterfaceReportsFlagReadAfterRestore(InlineOperandAbi abi)
    {
        byte[] prg = new byte[32 * BankSize];
        Place(prg, 0x10, 0x8000, 0x08, 0xA5, 0x40, 0x28, 0x90, 0x01, 0x60, 0x60);
        Dictionary<int, BankAnalysis> analyses = Analyze(prg, abi, (0x10, 0x8000));
        RoutineInterface result = Program.AnalyzeRoutineInterface(0x10, 0x8000, prg, abi, analyses);
        return result.EntryFlagReads.SetEquals(["C"])
            ? null
            : $"entry flag reads are [{string.Join(',', result.EntryFlagReads)}], expected C";
    }

    // SEC; BCC $8004; RTS; BMI $8006; RTS: the BCC is never taken, so the BMI is unreachable.
    private static string? EntryFlagsSkipInfeasibleBranch(InlineOperandAbi abi)
    {
        byte[] prg = new byte[32 * BankSize];
        Place(prg, 0x10, 0x8000, 0x38, 0x90, 0x01, 0x60, 0x30, 0x00, 0x60);
        Dictionary<int, BankAnalysis> analyses = Analyze(prg, abi, (0x10, 0x8000), (0x10, 0x8004));
        RoutineInterface result = Program.AnalyzeRoutineInterface(0x10, 0x8000, prg, abi, analyses);
        return result.EntryFlagReads.Count == 0
            ? null
            : $"entry flag reads are [{string.Join(',', result.EntryFlagReads)}], expected none";
    }

    // JSR $8010; BCC $8006; RTS; RTS, with PLA; PLA; RTS at $8010: the callee discards its return
    // address, so the carry test after the call is never reached through it.
    private static string? EntryFlagsStopAfterNonReturningCall(InlineOperandAbi abi)
    {
        byte[] prg = new byte[32 * BankSize];
        Place(prg, 0x10, 0x8000, 0x20, 0x10, 0x80, 0x90, 0x01, 0x60, 0x60);
        Place(prg, 0x10, 0x8010, 0x68, 0x68, 0x60);
        Dictionary<int, BankAnalysis> analyses = Analyze(prg, abi, (0x10, 0x8000));
        RoutineInterface result = Program.AnalyzeRoutineInterface(0x10, 0x8000, prg, abi, analyses);
        return result.EntryFlagReads.Count == 0
            ? null
            : $"entry flag reads are [{string.Join(',', result.EntryFlagReads)}], expected none";
    }

    // PHP; TSX; LDA #$00; STA $0101,X; PLP; BCC $800B; RTS; RTS: the saved status byte is overwritten
    // through the stack page, so PLP does not restore the caller's carry.
    private static string? EntryFlagsDistrustAliasedStack(InlineOperandAbi abi)
    {
        byte[] prg = new byte[32 * BankSize];
        Place(prg, 0x10, 0x8000, 0x08, 0xBA, 0xA9, 0x00, 0x9D, 0x01, 0x01, 0x28, 0x90, 0x01, 0x60, 0x60);
        Dictionary<int, BankAnalysis> analyses = Analyze(prg, abi, (0x10, 0x8000));
        RoutineInterface result = Program.AnalyzeRoutineInterface(0x10, 0x8000, prg, abi, analyses);
        return result.EntryFlagReads.Count != 0
            ? $"entry flag reads are [{string.Join(',', result.EntryFlagReads)}], expected none"
            : !result.EntryFlagsUnresolved.SetEquals(["C", "N", "V", "Z"])
                ? $"unresolved entry flags are [{string.Join(',', result.EntryFlagsUnresolved)}], expected C,N,V,Z"
                : null;
    }

    // BRK $82,$E7; RTS: the service receives the caller's carry and overflow, and nothing here shows
    // whether it reads them.
    private static string? EntryFlagsUnresolvedAtBrk(InlineOperandAbi abi)
    {
        byte[] prg = new byte[32 * BankSize];
        Place(prg, 0x08, 0x8000, 0x00, 0x82, 0xE7, 0x60);
        Dictionary<int, BankAnalysis> analyses = Analyze(prg, abi, (0x08, 0x8000));
        RoutineInterface result = Program.AnalyzeRoutineInterface(0x08, 0x8000, prg, abi, analyses);
        return result.EntryFlagReads.Count != 0
            ? $"entry flag reads are [{string.Join(',', result.EntryFlagReads)}], expected none"
            : !result.EntryFlagsUnresolved.SetEquals(["C", "V"])
                ? $"unresolved entry flags are [{string.Join(',', result.EntryFlagsUnresolved)}], expected C,V"
                : null;
    }

    // PHP; BRK $82,$E7; PLP; BCC $8008; RTS; RTS: the flags saved before the service are restored
    // after it, so the carry test reads the caller's carry.
    private static string? EntryFlagsResumeAfterBrk(InlineOperandAbi abi)
    {
        byte[] prg = new byte[32 * BankSize];
        Place(prg, 0x08, 0x8000, 0x08, 0x00, 0x82, 0xE7, 0x28, 0x90, 0x01, 0x60, 0x60);
        Dictionary<int, BankAnalysis> analyses = Analyze(prg, abi, (0x08, 0x8000));
        RoutineInterface result = Program.AnalyzeRoutineInterface(0x08, 0x8000, prg, abi, analyses);
        return result.EntryFlagReads.SetEquals(["C"])
            ? null
            : $"entry flag reads are [{string.Join(',', result.EntryFlagReads)}], expected C";
    }

    // Bank $08 JMP $C100 reaches STA $1F; RTS in fixed bank $0F; the write belongs to the body.
    private static string? InterfaceFollowsCrossBankTailJump(InlineOperandAbi abi)
    {
        byte[] prg = new byte[32 * BankSize];
        Place(prg, 0x08, 0x8000, 0x4C, 0x00, 0xC1);
        Place(prg, 0x0F, 0xC100, 0x85, 0x1F, 0x60);
        Dictionary<int, BankAnalysis> analyses = Analyze(prg, abi, (0x08, 0x8000));
        RoutineInterface result = Program.AnalyzeRoutineInterface(0x08, 0x8000, prg, abi, analyses);
        return result.DirectMemoryWrites.Contains("$1F")
            ? null
            : $"writes are [{string.Join(',', result.DirectMemoryWrites)}], expected $1F";
    }

    // BRK $82,$E7; JMP ($0000)
    private static string? InterfaceListsServicesAndUnfollowedJumps(InlineOperandAbi abi)
    {
        byte[] prg = new byte[32 * BankSize];
        Place(prg, 0x08, 0x8000, 0x00, 0x82, 0xE7, 0x6C, 0x00, 0x00);
        Dictionary<int, BankAnalysis> analyses = Analyze(prg, abi, (0x08, 0x8000));
        RoutineInterface result = Program.AnalyzeRoutineInterface(0x08, 0x8000, prg, abi, analyses);
        return !result.BrkServices.SetEquals(["$82/$E7"])
            ? $"BRK services are [{string.Join(',', result.BrkServices)}], expected $82/$E7"
            : !result.UnfollowedJumps.SetEquals(["($0000)"])
                ? $"unfollowed jumps are [{string.Join(',', result.UnfollowedJumps)}], expected ($0000)"
                : null;
    }

    // NOP; NOP; NOP; NOP; RTS seeded as windows $8000-$8000 and $8000-$8003.
    private static string? WindowsFromOneAddressAreOrderIndependent(InlineOperandAbi abi)
    {
        byte[] prg = new byte[32 * BankSize];
        Place(prg, 0x10, 0x8000, 0xEA, 0xEA, 0xEA, 0xEA, 0x60);
        CodeSeed narrow = new(0x10, 0x8000, 0x8001, "Self-test", "narrow window");
        CodeSeed wide = new(0x10, 0x8000, 0x8004, "Self-test", "wide window");
        string forward = Decoded(CodeAnalyzer.Analyze(prg, [narrow, wide], [], abi));
        string backward = Decoded(CodeAnalyzer.Analyze(prg, [wide, narrow], [], abi));
        return forward != backward ? $"decoded {forward} narrow-first but {backward} wide-first"
            : forward != "$8000,$8001,$8002,$8003" ? $"decoded {forward}, expected $8000-$8003"
            : null;
    }

    // NOP; NOP; NOP; RTS seeded as window $8000-$8001 and as an unbounded entry at $8000.
    private static string? WindowAndEntryAreOrderIndependent(InlineOperandAbi abi)
    {
        byte[] prg = new byte[32 * BankSize];
        Place(prg, 0x10, 0x8000, 0xEA, 0xEA, 0xEA, 0x60);
        CodeSeed window = new(0x10, 0x8000, 0x8002, "Self-test", "window");
        CodeSeed entry = new(0x10, 0x8000, null, "Self-test", "entry");
        string forward = Decoded(CodeAnalyzer.Analyze(prg, [window, entry], [], abi));
        string backward = Decoded(CodeAnalyzer.Analyze(prg, [entry, window], [], abi));
        return forward != backward ? $"decoded {forward} window-first but {backward} entry-first"
            : forward != "$8000,$8001,$8002,$8003" ? $"decoded {forward}, expected $8000-$8003"
            : null;
    }

    private static string Decoded(Dictionary<int, BankAnalysis> analyses) =>
        string.Join(',', analyses[0x10].Instructions.Values.Select(instruction => $"${instruction.Address:X4}"));

    private static string? CitationRejectsUndecodedIndirect(InlineOperandAbi abi)
    {
        byte[] prg = new byte[32 * BankSize];
        Dictionary<int, BankAnalysis> analyses = Analyze(prg, abi);
        return Rejects(prg, analyses, "LDA ($08),Y at $FFFF")
            ? !Rejects(prg, analyses, "LDA $0800 at $FFFF") ? "the direct-operand control was accepted" : null
            : "bogus citation 'LDA ($08),Y at $FFFF' was accepted";
    }

    // LDA ($08),Y; STA ($04,X); JMP ($0000)
    private static string? CitationAcceptsDecodedIndirect(InlineOperandAbi abi)
    {
        byte[] prg = new byte[32 * BankSize];
        Place(prg, 0x12, 0x8FD5, 0xB1, 0x08, 0x81, 0x04, 0x6C, 0x00, 0x00);
        Dictionary<int, BankAnalysis> analyses = Analyze(prg, abi, (0x12, 0x8FD5));
        foreach (string citation in new[]
        {
            "LDA ($08),Y at $8FD5", "STA ($04,X) at $8FD7", "JMP ($0000) at $8FD9", "LDA ($08),Y / STA ($04,X) at $8FD5-$8FD7"
        })
        {
            if (Rejects(prg, analyses, citation))
            {
                return $"decoded citation '{citation}' was rejected";
            }
        }

        return Rejects(prg, analyses, "LDA ($09),Y at $8FD5") ? null : "wrong-operand citation 'LDA ($09),Y at $8FD5' was accepted";
    }

    private static string? CitationRejectsUnparseableText(InlineOperandAbi abi)
    {
        byte[] prg = new byte[32 * BankSize];
        Place(prg, 0x12, 0x8FD5, 0xB1, 0x08, 0x60);
        Dictionary<int, BankAnalysis> analyses = Analyze(prg, abi, (0x12, 0x8FD5));
        return !Rejects(prg, analyses, "LDA [$08],Y at $8FD5") ? "malformed citation 'LDA [$08],Y at $8FD5' was accepted"
            : Rejects(prg, analyses, "Twenty record pointers loaded into $0C/$0D at $8FD5") ? "prose without a mnemonic was rejected"
            : null;
    }

    // LDA ($08),Y; RTS at $8FD5. Each rejected text names an instruction the validator would otherwise
    // never compare with the decoded code.
    private static string? CitationRejectsUncitedMnemonics(InlineOperandAbi abi)
    {
        byte[] prg = new byte[32 * BankSize];
        Place(prg, 0x12, 0x8FD5, 0xB1, 0x08, 0x60);
        Dictionary<int, BankAnalysis> analyses = Analyze(prg, abi, (0x12, 0x8FD5));
        foreach (string text in new[]
        {
            "LDA ($09), Y at $8FD5",
            "LDA [$00] / LDA ($08),Y at $8FD5",
            "LDA ($08),Y at $8FD5F",
            "LDA ($08),Y at $8FD",
            "lda ($09),y at $8fd5",
            "the table is read after LDA ($09),Y masks it",
            "PHP / ORA $06 at $8FD5",
        })
        {
            if (!Rejects(prg, analyses, text))
            {
                return $"'{text}' was accepted";
            }
        }

        foreach (string text in new[]
        {
            "LDA ($08),Y at $8FD5; the eight-bit count and $0C/$0D at $8FD5 follow",
            "LDA ($08),Y / RTS at $8FD5-$8FD7",
        })
        {
            if (Rejects(prg, analyses, text))
            {
                return $"'{text}' was rejected";
            }
        }

        return null;
    }

    // BRK $82,$E7; RTS in bank $08, cited from an owner in bank $12.
    private static string? CitationChecksBrkOperands(InlineOperandAbi abi)
    {
        byte[] prg = new byte[32 * BankSize];
        Place(prg, 0x08, 0x8000, 0x00, 0x82, 0xE7, 0x60);
        Dictionary<int, BankAnalysis> analyses = Analyze(prg, abi, (0x08, 0x8000));
        foreach (string text in new[]
        {
            "BRK at bank $08:$8000", "BRK service at bank $08:$8000", "BRK $82,$E7 at bank $08:$8000",
            "BRK service $82,$E7 / RTS at bank $08:$8000-$8003",
        })
        {
            if (Rejects(prg, analyses, text))
            {
                return $"'{text}' was rejected";
            }
        }

        foreach (string text in new[]
        {
            "BRK $82,$E6 at bank $08:$8000", "BRK $82,$E7,$60 at bank $08:$8000", "BRK at bank $08:$8001",
            "the BRK dispatcher", "BRK $82,$E7 before code resumes",
        })
        {
            if (!Rejects(prg, analyses, text))
            {
                return $"'{text}' was accepted";
            }
        }

        return null;
    }

    private static bool Rejects(byte[] prg, IReadOnlyDictionary<int, BankAnalysis> analyses, string reason)
    {
        try
        {
            CitationValidator.Validate([(0x12, "self-test", reason)], prg, analyses);
            return false;
        }
        catch (InvalidDataException)
        {
            return true;
        }
    }

    private static Dictionary<int, BankAnalysis> Analyze(
        byte[] prg,
        InlineOperandAbi abi,
        params (int Bank, int Address)[] seeds) =>
        CodeAnalyzer.Analyze(
            prg,
            seeds.Select(seed => new CodeSeed(seed.Bank, seed.Address, null, "Self-test", "synthetic probe")).ToList(),
            [],
            abi);

    private static void Place(byte[] prg, int bank, int address, params byte[] bytes) =>
        bytes.CopyTo(prg, (bank * BankSize) + address - CodeAnalyzer.CpuBase(bank));
}
